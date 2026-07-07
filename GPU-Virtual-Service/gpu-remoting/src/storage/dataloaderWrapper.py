import logging
import sys
import socket
import mmap
import posix_ipc
import torch
import os
import struct
from torch.utils.data import DataLoader, Dataset
from torch import nn
from torchvision import datasets
from torchvision.transforms import ToTensor
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from common import DataloaderInfo, BatchInfo, get_server_ep, get_client_id, logging, send_msg, recv_msg, EPOCH_REQ, EPOCH_STOP, DATALOADER_REQ
import torch.distributed as dist

from accelerate import Accelerator
import transformers
from transformers import (
    AutoConfig,
    AutoModelForSequenceClassification,
    AutoTokenizer,
    DataCollatorWithPadding,
    PretrainedConfig,
    SchedulerType,
    default_data_collator,
    get_scheduler,
)


# logging.basicConfig(level=logging.INFO, format='%(asctime)s [%(levelname)s] %(message)s', datefmt='%Y-%m-%d %H:%M:%S')
# REQUEST_TIMEOUT = 5000
REQUEST_RETRIES = 3

pid = os.getpid()

class ClientContext:
    def __init__(self, server_ip, server_port, identity):
        self.identity = identity
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.socket.connect((server_ip, server_port))
    
    def submit_dataloaderinfo(self, is_train, dataloader_info):
        send_msg(self.socket, (DATALOADER_REQ, is_train, self.identity, dataloader_info))

    def submit_epoch_req(self, is_train, epoch_idx):
        send_msg(self.socket, (EPOCH_REQ, is_train, epoch_idx))
    
    def stop_epoch(self, is_train, batch_idx):
        send_msg(self.socket, (EPOCH_STOP, is_train, batch_idx))

    def recv_dataset_metadata(self):
        return recv_msg(self.socket)


client = None

class FlexGVirtDataLoader(): 
    def __init__(self, train=True,                                 # train or test
                 remote_dataset_path=None, regular_dataset=None,   # remote dataset filepath or PyTorch built-in datasets 
                 batch_size=1,                                     # number of samples per batch        
                 shuffle=False,                                    # whether to shuffle the data at every epoch
                 sampler=None,                                     # defines the strategy to draw samples from the dataset
                 batch_sampler=None,                               # defines the strategy to draw batches from the dataset (SequentialSampler, RandomSampler, WeightedSampler, etc.)
                 collate_fn=None,                                  # merges a list of samples to form a mini-batch of Tensor(s) (e.g. image, label pairs)
                 drop_last=False,                                  # whether to drop the last incomplete batch, if the dataset size is not divisible by the batch size
                 preprocess=None,                                  # preprocessing function for each sample
                 generator=None,                                   # random number generator 
                 num_workers=0,                                    # number of subprocesses to use for dataloading
                 other_dataloader=None,                            # other dataloader
                 proxy=True,                                       # whether to use GPU proxy & Resource Dispatcher
                 distributed=False,                                # whether to use distributed data parallelism
                 seed=0,                                           # random seed used to shuffle the sampler, identical across all processes in the distributed group
                 rank=None,                                        # rank of the current process in the distributed group
                 world_size=None,                                  # number of processes in the distributed group
                 sample_drop_last=False,                           # If ``False``, the sampler will add extra indices to make the data evenly divisible across the replicas.
                 ):                                       
        torch.cuda.mem_get_info() # todo
        clientId = get_client_id()
        identity = str(clientId) + "_" + str(pid)
        logging.debug(f"dataloader identity: {identity}")
        self.batch_num              = 0
        self.train                  = train

        batch_info_bytes            = struct.calcsize('bN') #! be careful with the alignment
        self.shm_obj = posix_ipc.SharedMemory(name=("/flexgv_shm_%s_datatype" % identity), flags=posix_ipc.O_CREAT|posix_ipc.O_RDWR, size=(batch_info_bytes + 4 + 40)) # clientID_PID_datatype (contains: struct batch info + server ip and port)
        self.shm_desc = mmap.mmap(self.shm_obj.fd, self.shm_obj.size)
        self.shm_write_byte = (1 if self.train else 2).to_bytes(1, byteorder='little') # 1 for train, 2 for test/validation, 3 for model
        self.shm_desc.seek(0)
        self.shm_desc.write((0).to_bytes(1, byteorder='little')) # init the data type

        global client
        if client is None:
            if proxy:
                self.shm_desc.seek(batch_info_bytes)
                server_ip_port = self.shm_desc.read(4 + 40) # read server ip and port
                server_port, server_ip_bytes = struct.unpack('i40s', server_ip_port)
                server_ip = server_ip_bytes.decode('utf-8').strip('\x00')
            else: # todo: to be validated
                server_ip, server_port = get_server_ep(need_clientId = True)
            client = ClientContext(server_ip, server_port, identity)

        self.remote_dataset_path    = remote_dataset_path
        self.dataset                = regular_dataset
        self.batch_size             = batch_size
        self.shuffle                = shuffle   
        self.sampler                = sampler
        self.batch_sampler          = batch_sampler
        self.collate_fn             = collate_fn
        self.drop_last              = drop_last
        self.preprocess             = preprocess
        self.generator              = generator
        self.num_workers            = num_workers
        
        self.distributed            = distributed
        self.seed                   = seed
        self.epoch                  = 0
        if self.distributed:
            if world_size is None:
                if not dist.is_available():
                    raise RuntimeError("Requires distributed package to be available")
                world_size = dist.get_world_size()
            if rank is None:
                if not dist.is_available():
                    raise RuntimeError("Requires distributed package to be available")
                rank = dist.get_rank()
            if rank >= world_size or rank < 0:
                raise ValueError(
                    "Invalid rank {}, rank should be in the interval"
                    " [0, {}]".format(rank, world_size - 1))

            self.rank               = rank
            self.world_size         = world_size
            logging.debug(f"rank={self.rank}, world_size={self.world_size}")

        logging.debug("MyDataLoader __init__ called")
        client.submit_dataloaderinfo(self.train, DataloaderInfo(train, remote_dataset_path, regular_dataset,
                                                                batch_size, shuffle, sampler, batch_sampler, 
                                                                collate_fn, drop_last, preprocess, 
                                                                generator, num_workers, other_dataloader, 
                                                                distributed, seed, rank, world_size, sample_drop_last))
        # self.batch_info             = client.recv_batch_info()
        self.dataset_metadata       = client.recv_dataset_metadata()
        self.batch_num              = self.dataset_metadata.batch_num
        self.stop_batch_idx         = self.batch_num
        self.set_epoch(0)

        logging.debug(f"Received dataset metadata(batch info): {self.dataset_metadata}")
    
    def set_epoch(self, epoch):
        self.epoch = epoch
        logging.debug(f"set_epoch called, epoch={self.epoch}")

    def stop(self):
        self.stop_batch_idx = self.batch_idx # store the current batch index
        self.batch_idx = self.batch_num # to stop the iteration

    def __iter__(self):
        logging.debug(f"iter__init__ called, is_train={self.train}")
        self.batch_idx = 0
        self.shm_desc.seek(0)
        self.shm_desc.write(self.shm_write_byte)           # notify the CUDA wrapper to start the data loading process
        global client
        client.submit_epoch_req(self.train, self.epoch)          # notify the server to start the data loading process
        # self.batch_info             = client.recv_batch_info() # recv batch info again just for blocking the process
        self.dataset_metadata       = client.recv_dataset_metadata() # recv dataset metadata again just for blocking the process
        self.batch_num              = self.dataset_metadata.batch_num
        self.stop_batch_idx         = self.batch_num
        
        self.first_data             = () if self.dataset_metadata.is_iterable else {}
        self.first_batch_size            = 0
        for col in self.dataset_metadata.first_batch:
            data = torch.zeros(col["shape"], dtype=col["dtype"])
            self.first_batch_size += data.numel() * data.element_size()
            if self.dataset_metadata.is_iterable:
                self.first_data += (data,)
            else:
                self.first_data[col["key"]] = data
        
        self.last_data              = () if self.dataset_metadata.is_iterable else {}
        self.last_batch_size             = 0
        for col in self.dataset_metadata.last_batch:
            data = torch.zeros(col["shape"], dtype=col["dtype"])
            self.last_batch_size += data.numel() * data.element_size()
            if self.dataset_metadata.is_iterable:
                self.last_data += (data,)
            else:
                self.last_data[col["key"]] = data
        
        logging.debug(f"first_batch_size: {self.first_batch_size}, last_batch_size: {self.last_batch_size}")

        return self
    
    def __next__(self):
        logging.debug(f"iter__next__ called, batch_idx={self.batch_idx}, batch_num={self.batch_num}")
        if self.batch_idx < self.batch_num - 1:
            self.batch_idx += 1
            self.shm_desc.seek(0)
            self.shm_desc.write(struct.pack('bN', (1 if self.train else 2), self.first_batch_size))
            return self.first_data
        elif self.batch_idx == self.batch_num - 1:
            self.batch_idx += 1
            self.shm_desc.seek(0)
            self.shm_desc.write(struct.pack('bN', (1 if self.train else 2), self.last_batch_size))
            return self.last_data
        else:
            self.shm_desc.seek(0)
            self.shm_desc.write((0).to_bytes(1, byteorder='little')) # clear the data type
            global client
            client.stop_epoch(self.train, self.stop_batch_idx) # batch_idx indicates the next batch to be loaded
            raise StopIteration

    def __len__(self):
        return self.batch_num
    
    def __del__(self):
        self.shm_desc.close()
        self.shm_obj.close_fd()


