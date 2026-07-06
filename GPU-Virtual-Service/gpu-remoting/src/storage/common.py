import json
import torch
import ctypes
import logging
import pickle
import struct

logging.basicConfig(level=logging.INFO, format='%(asctime)s [%(levelname)s] %(message)s', datefmt='%Y-%m-%d %H:%M:%S')
logging.getLogger("PIL.TiffImagePlugin").setLevel(51)

EPOCH_STOP = 0
DATALOADER_REQ = 1
EPOCH_REQ = 2

class DataloaderInfo:
    def __init__(self, train=True, remote_dataset_path=None, regular_dataset=None, batch_size=1, shuffle=False, sampler=None, batch_sampler=None, collate_fn=None, drop_last=False, preprocess=None, generator=None, num_workers=0, other_dataloader=None, distributed=False, seed=0, rank=0, world_size=1, sample_drop_last=False):
        self.train = train
        self.remote_dataset_path = remote_dataset_path
        self.regular_dataset = regular_dataset
        self.batch_size = batch_size
        self.shuffle = shuffle
        self.sampler = sampler
        self.batch_sampler = batch_sampler
        self.collate_fn = collate_fn
        self.drop_last = drop_last
        self.preprocess = preprocess
        self.generator = generator
        self.num_workers = num_workers
        self.other_dataloader = other_dataloader
        self.distributed = distributed
        self.seed = seed
        self.rank = rank
        self.world_size = world_size
        self.sample_drop_last = sample_drop_last
    
    def __str__(self):
        return f"\nDataloaderInfo(train: {self.train}, remote_dataset_path: {self.remote_dataset_path}, batch_size: {self.batch_size}, shuffle: {self.shuffle}, sampler: {self.sampler}, batch_sampler: {self.batch_sampler}, collate_fn: {self.collate_fn}, drop_last: {self.drop_last}, preprocess: {self.preprocess}), generator: {self.generator}, num_workes: {self.num_workers},\ndistributed: {self.distributed}, seed: {self.seed}, rank: {self.rank}, world_size: {self.world_size}, sample_drop_last: {self.sample_drop_last},\nregular_dataset: {self.regular_dataset},\nother_dataloader: {self.other_dataloader}"

class BatchInfo:
    def __init__(self, train, batch_num, data_dtype=None, label_dtype=None, first_data_shape=None, first_label_shape=None, last_data_shape=None, last_label_shape=None):
        self.train = train
        self.batch_num = batch_num
        self.data_dtype = data_dtype
        self.label_dtype = label_dtype
        self.first_data_shape = first_data_shape
        self.first_label_shape = first_label_shape
        self.last_data_shape = last_data_shape
        self.last_label_shape = last_label_shape
        

    def __str__(self):
        return f"BatchInfo(train: {self.train}, batch_num: {self.batch_num}, data_dtype: {self.data_dtype}, label_dtype: {self.label_dtype}, first_data_shape: {self.first_data_shape}, first_label_shape: {self.first_label_shape}, last_data_shape: {self.last_data_shape}, last_label_shape: {self.last_label_shape}"

class DatasetMetadata:
    def __init__(self, train, batch_num, is_iterable, first_batch_metadata, last_batch_metadata):
        self.train = train
        self.batch_num = batch_num
        self.is_iterable = is_iterable
        self.first_batch = first_batch_metadata
        self.last_batch = last_batch_metadata
    
    def __str__(self):
        return f"DatasetMetadata(train: {self.train}, batch_num: {self.batch_num}, is_iterable:{self.is_iterable} ,first_batch: {self.first_batch}, last_batch: {self.last_batch}"

    
def get_server_ep():
    with open('config.json') as f:
        config = json.load(f)
    server_port = config['DatasetHandlerConfig']['handlerPort_']
    server_ip = config['DatasetHandlerConfig']['handlerIp_']
    return server_ip, server_port

def get_client_id():
    with open('config.json') as f:
        config = json.load(f)
    return config['ClientConfig']['clientID_']
    
def get_size_t_len():
    return ctypes.sizeof(ctypes.c_size_t)

def send_msg(socket, msg):
    msg_byte = pickle.dumps(msg)
    msg_len = len(msg_byte)
    length_data = struct.pack('Q', msg_len)
    socket.sendall(length_data)
    socket.sendall(msg_byte)


def recv_msg(socket):
    length_data = socket.recv(get_size_t_len())
    if not length_data:
        return None
    msg_len = struct.unpack('Q', length_data)[0]
    msg_byte = bytearray(msg_len)
    view = memoryview(msg_byte)

    bytes_received = 0
    while bytes_received < msg_len:
        packet_size = socket.recv_into(view[bytes_received:])
        if not packet_size:
            return None
        bytes_received += packet_size

    return pickle.loads(msg_byte)