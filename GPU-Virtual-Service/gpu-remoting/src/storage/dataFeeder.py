import threading
import numpy as np
from PyShmqueue import *
import torch
import mmap
import posix_ipc
from torch.utils.data import DataLoader
# from torchvision import datasets
import torchvision
from common import DatasetMetadata, get_size_t_len, logging, DataloaderInfo
# from datasets import load_dataset, load_from_disk
import transformers
from transformers import default_data_collator

class DataFeeder:
    def __init__(self, ident="163_1234", identNum=1, 
                 dataloaderInfo: DataloaderInfo = None):
        torch.set_num_threads(8)
        self.train = dataloaderInfo.train
        self.ident = ident
        self.identNum = identNum
        self.stop_flag = threading.Event()
        self.thread = None

        self.distributed = dataloaderInfo.distributed

        if dataloaderInfo.other_dataloader is not None:
            self.dataloader = dataloaderInfo.other_dataloader
        else:
            # from datasets import load_from_disk
            # if "ImageNet" in dataloaderInfo.remote_dataset_path:
            #     dataset_path = "/mnt/nvme0/FlexGV_Test/ImageNet-1K/{}".format("train" if self.train else "val")
            #     self.dataset = torchvision.datasets.ImageFolder(dataset_path, transform=dataloaderInfo.preprocess)
            # elif "GLUE" in dataloaderInfo.remote_dataset_path:
            #     dataset_path = "/mnt/nvme0/FlexGV_Test/GLUE/MRPC_Processed"
            #     local_prep_dataset = load_from_disk(dataset_path)
            #     self.dataset  = local_prep_dataset["train"] if self.train else local_prep_dataset["validation"]
            if dataloaderInfo.regular_dataset is not None:
                self.dataset = dataloaderInfo.regular_dataset
            else:
                self.dataset = torchvision.datasets.ImageFolder(dataloaderInfo.remote_dataset_path, transform=dataloaderInfo.preprocess)

            if self.distributed:
                dataloaderInfo.sampler = torch.utils.data.distributed.DistributedSampler(dataset=self.dataset, 
                                                                          num_replicas=dataloaderInfo.world_size,
                                                                          rank=dataloaderInfo.rank, shuffle=dataloaderInfo.shuffle, 
                                                                          seed=dataloaderInfo.seed, drop_last=dataloaderInfo.sample_drop_last)

            self.dataloader = DataLoader(self.dataset, batch_size=dataloaderInfo.batch_size, 
                                            shuffle=(dataloaderInfo.sampler is None), 
                                            sampler=dataloaderInfo.sampler, 
                                            batch_sampler=dataloaderInfo.batch_sampler, 
                                            collate_fn=dataloaderInfo.collate_fn, 
                                            drop_last=dataloaderInfo.drop_last, 
                                            generator=dataloaderInfo.generator, num_workers=dataloaderInfo.num_workers)
        self.client_batch_cnt = len(self.dataloader)
        self.dataloader_iter = iter(self.dataloader)
        self.is_iterable = True
        self.cur_batch_index = 0
        self.queue = None
        self.shm_obj = None
        self.shm_desc = None
        self.queue_batch_num = 2

    def reset(self, epoch = 0):
        if self.distributed:
            self.dataloader.sampler.set_epoch(epoch)
        self.dataloader_iter = iter(self.dataloader)
        self.cur_batch_index = 0
        self.client_batch_cnt = len(self.dataloader)
        logging.debug(f"DataFeeder reset: isTrain: {self.train}, client_batch_cnt: {self.client_batch_cnt}")
        if self.queue is not None:
            self.queue.clear()
            self.queue.ready()

            
    def _create_shm_queue(self, first_batch_data):
        size_t_len = get_size_t_len()
        # queue_size = self.queue_batch_num * (data.numel()*data.element_size()+label.numel()*label.element_size() + 2*size_t_len)
        first_batch_size = 0
        if self.is_iterable == False:
            first_batch_data = first_batch_data.values()
        for value in first_batch_data:
            # first_batch_size += (value.numel()*value.element_size() + size_t_len)
            first_batch_size += (value.numel()*value.element_size() + size_t_len * 2)
        queue_size = self.queue_batch_num * first_batch_size

        # record the size of shared memory queue
        data_type = 1 if self.train else 2
        # self.shm_obj = posix_ipc.SharedMemory(name=("/flexgv_shm_%d_%d" % (self.client_id, data_type)), 
        #                                       flags=posix_ipc.O_CREAT, size=size_t_len)
        self.shm_obj = posix_ipc.SharedMemory(name=("/flexgv_shm_%s_%d" % (self.ident, data_type)), # clientID_PID_1/2
                                              flags=posix_ipc.O_CREAT, size=size_t_len * 3)
        self.shm_desc = mmap.mmap(self.shm_obj.fd, self.shm_obj.size)
        self.shm_desc.seek(0)
        logging.debug(f"first_batch_size={first_batch_size}, queue_size={queue_size}, client_batch_cnt={self.client_batch_cnt}, identNum={self.identNum}, ident={self.ident}")
        self.shm_desc.write(queue_size.to_bytes(size_t_len, byteorder='little'))
        self.shm_desc.write(self.client_batch_cnt.to_bytes(size_t_len, byteorder='little'))
        self.shm_desc.write(self.identNum.to_bytes(size_t_len, byteorder='little'))

        # get the instance of shared memory queue
        self.queue = ShareMemoryQueue((self.identNum<<2) + data_type, 
                                          queue_size, ONE_READ_ONE_WRITE, 1) # 1 for creating a new queue
        logging.info(f"Shared Memory Queue with size: {queue_size} created for client({self.ident}), with data type = {data_type}")

    
    def get_batch_info(self, is_first=False):
        # first_batch_data, first_batch_label = next(iter(self.dataloader_iter))
        first_batch_data = next(iter(self.dataloader_iter))
        if isinstance(first_batch_data, dict):
            self.is_iterable = False

        first_batch_metadata, last_batch_metadata = [], []
        for col in first_batch_data:
            col_metadata = {}
            if self.is_iterable:
                col_metadata["key"] = None
            else:
                col_metadata["key"] = col
                col = first_batch_data[col]
            col_metadata["dtype"] = col.dtype
            col_metadata["shape"] = col.shape

            col_metadata_for_last = col_metadata.copy()
            if self.dataloader.drop_last == False:
                if hasattr(self.dataloader.sampler, 'num_samples'):  # DistributedSampler has num_samples
                    # number of samples that the current process should process
                    local_dataset_size = self.dataloader.sampler.num_samples
                    local_last_batch_size = local_dataset_size % self.dataloader.batch_size or self.dataloader.batch_size
                    col_metadata_for_last["shape"] = (local_last_batch_size, *col.shape[1:])
                else:
                    # single GPU
                    last_batch_size = len(self.dataloader.dataset) % self.dataloader.batch_size or self.dataloader.batch_size
                    col_metadata_for_last["shape"] = (last_batch_size, *col.shape[1:])
            
            first_batch_metadata.append(col_metadata)
            last_batch_metadata.append(col_metadata_for_last)

        # if self.dataloader.drop_last:
        #     last_data_shape, last_label_shape = first_batch_data.shape, first_batch_label.shape
        # else:
        #     last_batch_size = len(self.dataloader.dataset) % self.dataloader.batch_size or self.dataloader.batch_size
        #     last_data_shape = (last_batch_size, *first_batch_data.shape[1:])
        #     last_label_shape = (last_batch_size, *first_batch_label.shape[1:])
        
        if is_first == True:
            self._create_shm_queue(first_batch_data)
        else:
            if self.is_iterable == False:
                first_batch_data = first_batch_data.values()
            for value in first_batch_data:
                self.queue.write(value.cpu().numpy())
            logging.info(f"[client: {self.ident}] preload_1st_data (isTrain: {self.train}): batch_idx = {self.cur_batch_index} / {self.queue_batch_num - 1}")
            self.cur_batch_index += 1 # write the first batch
            self.preload_data() # write the next batches

        # return BatchInfo(self.train, len(self.dataloader), first_batch_data.dtype, first_batch_label.dtype, first_batch_data.shape, first_batch_label.shape, last_data_shape, last_label_shape)
        return DatasetMetadata(self.train, len(self.dataloader), self.is_iterable, first_batch_metadata, last_batch_metadata)
    
    def preload_data(self):
        while self.cur_batch_index < self.queue_batch_num:
            data = next(self.dataloader_iter)
            if self.is_iterable == False:
                data = data.values()
            for value in data:
                self.queue.write(value.cpu().numpy())
            logging.info(f"[client: {self.ident}] preload_data (isTrain: {self.train}): batch_idx = {self.cur_batch_index} / {self.queue_batch_num - 1}")
            self.cur_batch_index += 1 # write the second batch

    def load_data(self):
        # data_time_list = []
        while self.cur_batch_index < self.client_batch_cnt and not self.stop_flag.is_set():
            # start_time = time.time()
            data = next(self.dataloader_iter)
            # data_time = time.time() - start_time
            if self.is_iterable == False:
                data = data.values()
            for value in data:
                self.queue.write(value.cpu().numpy())
            # data_time_list.append(data_time)
            # for value in data:
            #     self.queue.read(value.numel()*value.element_size())
            # logging.info(f"data time: {data_time}")
            # logging.debug(f"[client: {self.ident}] load_data (isTrain: {self.train}): batch_idx = {self.cur_batch_index} / {self.client_batch_cnt - 1}")

            if (self.cur_batch_index % 10 == 0):
                logging.info(f"[client: {self.ident}] load_data (isTrain: {self.train}) for 10 batch: cur_batch_idx = {self.cur_batch_index} / {self.client_batch_cnt - 1}")
            self.cur_batch_index += 1
        logging.info(f"[client: {self.ident}] load_data: dataloader(isTrain: {self.train}) has been iterated over successfully, current batch index = {self.cur_batch_index - 1}")
        # logging.info(f"[client: {self.ident}] load_data: data time Avg: {sum(data_time_list)/len(data_time_list)}")
        # no need to reset the dataloader, since cudaMemcpyAsync may have not been finished
    
    def start_load_data(self):
        if self.thread is None or not self.thread.is_alive():
            self.stop_flag.clear()
            self.thread = threading.Thread(target=self.load_data)
            self.thread.start()
            logging.info(f"[{self.ident}] started loading data(isTrain={self.train})")
        else:
            logging.info(f"[{self.ident}] data loading thread(isTrain={self.train}) is already running")

    def stop_load_data(self, batch_num = 0):
        if self.thread is not None and self.thread.is_alive():
            if batch_num > 0:
                if self.cur_batch_index < batch_num: # wait for completing the batch_num
                    self.client_batch_cnt = batch_num
                else: # already iterated over the batch_num
                    self.stop_flag.set()
                    self.queue.clear() # avoid blocking in the writing queue
            else:
                self.stop_flag.set()
            self.thread.join()
            self.thread = None
            logging.info(f"[{self.ident}] stopped loading data(isTrain={self.train})")
        else:
            logging.info(f"[{self.ident}] data loading thread(isTrain={self.train}) has already been stopped")

    # def feed_data(self):
    #     data_type = 1 if self.train else 2
    #     queue_size = None
    #     data, label = next(self.dataloader_iter)
    #     if self.queue is None:
    #         queue_size = 2 * (data.numel()*data.element_size()+label.numel()*label.element_size() + 2*get_size_t_len())
    #         data_type = 1 if self.train else 2
    #         size_t_len = get_size_t_len()
    #         self.shm_obj = posix_ipc.SharedMemory(name=("/flexgv_shm_%d_%d" % (self.client_id, data_type)), 
    #                                              flags=posix_ipc.O_CREAT, size=size_t_len)
    #         self.shm_desc = mmap.mmap(self.shm_obj.fd, self.shm_obj.size)
    #         self.shm_desc.write(queue_size.to_bytes(size_t_len, byteorder='little'))
    #         self.queue = ShareMemoryQueue((self.client_id<<2) + data_type, 
    #                                       queue_size, ONE_READ_ONE_WRITE, 1) # 0 for not creating a new queue
    #         print(f"Queue with size: {queue_size} created for client{self.client_id}, with data type = {data_type}")
   
    #     # label_bytes = label.numpy().view(np.uint8)
    #     # print(f"is_train: {self.train} y_bytes[:8]:{label_bytes[:8].tobytes().hex()}, y_bytes[-8:]:{label_bytes[-8:].tobytes().hex()}")
    #     # self.queue.PrintTrunk()
    #     ret = self.queue.write(data.cpu().numpy())
    #     # print(f"data write ret: {ret}")
    #     ret = self.queue.write(label.cpu().numpy())
    #     # print(f"label write ret: {ret}")
    #     # ret = self.queue.write(label.cpu().numpy())
    #     # print(f"test third wirte: {ret}")
        
    #     res_idx = self.batch_index
    #     self.batch_index += 1
    #     if (self.batch_index % 50 == 0):
    #         print(f"HYF: feed_data: batch_idx: {self.batch_index} / {len(self.dataloader) - 1}")
    #     # if self.batch_index  >= len(self.dataloader):
    #     #     self.reset()

    #     # return BatchInfo(self.train, res_idx, data.shape, data.dtype, label.shape, label.dtype)
    
    def __del__(self):
        self.is_closed = False
        if self.shm_obj is not None:
            self.shm_desc.close()
            self.shm_obj.close_fd()
            # self.shm_obj.unlink() # unlinked by the CUDA handle