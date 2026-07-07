import copy
import ctypes
import os
import time
import numpy as np
from ctypes import *
CACHE_LINE_SIZE = 64
# Define QueueModule
ONE_READ_ONE_WRITE = 0   
ONE_READ_MUL_WRITE = 1   
MUL_READ_ONE_WRITE = 2   
MUL_READ_MUL_WRITE = 3 

SHAR_KEY_MODEL = 100010
SHAR_KEY_TRAIN = 100020
SHAR_KEY_VAL = 100030
SHAR_KEY_TEST = 100040


class stMemTrunk(Structure):
    _fields_ = [("m_iBegin", ctypes.c_uint),
            ("__cache_padding1__", ctypes.c_char * CACHE_LINE_SIZE),
            ("m_iEnd", ctypes.c_uint),
            ("__cache_padding2__", ctypes.c_char * CACHE_LINE_SIZE),
            ("m_iShmKey", ctypes.c_int),
            ("__cache_padding3__", ctypes.c_char * CACHE_LINE_SIZE),
            ("m_iSize", ctypes.c_uint),
            ("__cache_padding4__", ctypes.c_char * CACHE_LINE_SIZE),
            ("m_iShmId", ctypes.c_int),
            ("__cache_padding5__", ctypes.c_char * CACHE_LINE_SIZE),
            ("m_eQueueModule", ctypes.c_int)]
    
class CShmRWlock(ctypes.Structure):
    _fields_ = [("m_iSemID", ctypes.c_int),
                ("m_iSemKey", ctypes.c_int)]
    
class CMessageQueue(ctypes.Structure):
    _fields_ = [("m_stMemTrunk", ctypes.POINTER(stMemTrunk)),
                ("m_pBeginLock", ctypes.POINTER(CShmRWlock)),
                ("m_pEndLock", ctypes.POINTER(CShmRWlock)),
                ("m_pQueueAddr", ctypes.POINTER(ctypes.c_ubyte)),
                ("m_pShm", ctypes.c_void_p)]

print("[Python]: Start loading libshmmqueue.so...")
libLoad = ctypes.cdll.LoadLibrary
try:
    module_root_path = os.path.dirname(__file__)
    parent_of_parent_path = os.path.dirname(os.path.dirname(module_root_path)) 
    target_lib_path = os.path.join(parent_of_parent_path, "out", "lib64", "libshmmqueue.so")
    share = libLoad(target_lib_path)
    print("[Python]: Loading libshmmqueue.so successful!")
    # print(share.IsPowerOfTwo(3))
    # Define the function signatures
    share.CreateInstance.argtypes = [ctypes.c_int, ctypes.c_size_t, ctypes.c_int]
    share.CreateInstance.restype = ctypes.POINTER(CMessageQueue)

    share.GetInstance.argtypes = [ctypes.c_int, ctypes.c_size_t, ctypes.c_int]
    share.GetInstance.restype = ctypes.POINTER(CMessageQueue)

    share.DestroyInstance.argtypes = [ctypes.POINTER(CMessageQueue)]
    share.DestroyInstance.restype = None

    share.SendMessage.argtypes = [ctypes.POINTER(CMessageQueue), ctypes.POINTER(ctypes.c_ubyte), ctypes.c_size_t]
    share.SendMessage.restype = ctypes.c_int

    share.GetMessage.argtypes = [ctypes.POINTER(CMessageQueue), ctypes.POINTER(ctypes.c_ubyte)]
    share.GetMessage.restype = ctypes.c_int
    
    share.ReadMessage.argtypes = [ctypes.POINTER(CMessageQueue), ctypes.POINTER(ctypes.c_ubyte)]
    share.ReadMessage.restype = ctypes.c_int
    
    share.ReadHeadMessage.argtypes = [ctypes.POINTER(CMessageQueue), ctypes.POINTER(ctypes.c_ubyte)]
    share.ReadHeadMessage.restype = ctypes.c_int

    share.DeleteHeadMessage.argtypes = [ctypes.POINTER(CMessageQueue)]
    share.DeleteHeadMessage.restype = ctypes.c_int

    share.PrintTrunk.argtypes = [ctypes.POINTER(CMessageQueue)]
    share.PrintTrunk.restype = None

    share.GetFreeSize.argtypes = [ctypes.POINTER(CMessageQueue)]
    share.GetFreeSize.restype = ctypes.c_uint

    share.GetDataSize.argtypes = [ctypes.POINTER(CMessageQueue)]
    share.GetDataSize.restype = ctypes.c_uint

    share.GetQueueLength.argtypes = [ctypes.POINTER(CMessageQueue)]
    share.GetQueueLength.restype = ctypes.c_uint

    share.ClearQueue.argtypes = [ctypes.POINTER(CMessageQueue)]
    share.ClearQueue.restype = None

    share.InitLock.argtypes = [ctypes.POINTER(CMessageQueue)]
    share.InitLock.restype = None

    share.IsBeginLock.argtypes = [ctypes.POINTER(CMessageQueue)]
    share.IsBeginLock.restype = ctypes.c_int

    share.IsEndLock.argtypes = [ctypes.POINTER(CMessageQueue)]
    share.IsEndLock.restype = ctypes.c_int

    share.CreateShareMem.argtypes = [ctypes.c_int, ctypes.c_long, ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_int)]
    share.CreateShareMem.restype = ctypes.POINTER(ctypes.c_ubyte)

    share.DestroyShareMem.argtypes = [ctypes.c_void_p, ctypes.c_int]
    share.DestroyShareMem.restype = ctypes.c_int

    share.IsPowerOfTwo.argtypes = [ctypes.c_size_t]
    share.IsPowerOfTwo.restype = ctypes.c_int

    share.Fls.argtypes = [ctypes.c_size_t]
    share.Fls.restype = ctypes.c_int

    share.RoundupPowofTwo.argtypes = [ctypes.c_size_t]
    share.RoundupPowofTwo.restype = ctypes.c_size_t
    # share.get_share_body_address.restype = ctypes.POINTER(ctypes.c_uint8)
except Exception as e:
    print("[Python]: Failed to load shared memory dynamic library! Probably due to not placing libshmmqueue.so in out/lib64")
    print("[Python]: Detailed error messages-" + str(e))
    exit(1)




class ShareMemoryQueue:
    def __init__(self, shmkey, queuesize, queueModule, CREATE_FLAG):
        self.stop_blocking = False
        self.shmkey = shmkey
        # get existed ShareMemoryQueue
        if(CREATE_FLAG):
            self.message_queue_ptr = share.CreateInstance(shmkey, queuesize, queueModule)
        else:
            self.message_queue_ptr = share.GetInstance(shmkey, queuesize, queueModule)
    
    def write(self, batch):
        data_ptr = batch.ctypes.data
        bytes_ptr = ctypes.cast(data_ptr, ctypes.POINTER(ctypes.c_ubyte))
        error_code = share.SendMessage(self.message_queue_ptr, bytes_ptr, batch.nbytes)
        print_flag = False
        # self.stop_blocking = False
        while error_code !=0:
            if self.stop_blocking == True:
                break
            if not print_flag:
                print(f"[shmkey: {self.shmkey}] write failed, error_code is {error_code}, Write blocking...")
                print_flag = True
            error_code = share.SendMessage(self.message_queue_ptr, bytes_ptr, batch.nbytes)
        # print(f"write  #batch_idx-{batch_idx} success")
        # bytes_batch = batch.tobytes()
        # data_ptr = ctypes.cast(ctypes.create_string_buffer(bytes_batch), ctypes.POINTER(ctypes.c_ubyte))
        # res = share.SendMessage(self.message_queue_ptr, data_ptr, len(bytes_batch))
        return error_code
    
    def get(self):
        buffer_size = 100  # Replace with the desired buffer size
        buffer = np.zeros(buffer_size, dtype=np.uint8)
        result = share.GetMessage(self.message_queue_ptr, buffer.ctypes.data_as(POINTER(c_ubyte)))
        print("Received data: ", buffer[:result])
    
    def read(self, buffer_size):
        # buffer_size = 100  # Replace with the desired buffer size
        buffer = np.zeros(buffer_size, dtype=np.uint8)
        result = share.ReadMessage(self.message_queue_ptr, buffer.ctypes.data_as(POINTER(c_ubyte)))
        # print("Read data: ", buffer)
        
    def ready(self):
        self.stop_blocking = False
    
    def clear(self):
        self.stop_blocking = True
        share.ClearQueue(self.message_queue_ptr)
        print(f"[shmkey: {self.shmkey}] clear queue, stop blocking = {self.stop_blocking}")
       
    def GetDataSize(self):
        return share.GetDataSize(self.message_queue_ptr)

    def PrintTrunk(self):
        share.PrintTrunk(self.message_queue_ptr)
    
    def getQueueLength(self):
        share.GetQueueLength(self.message_queue_ptr)
    
    # def __del__(self):
    #     share.DestroyInstance(self.message_queue_ptr)
    #     print("Destroy ShareMemoryQueue")


def test():
    shmkey = 100010
    queuesize = 10240
    queueModule = ONE_READ_ONE_WRITE
    shmqueue = ShareMemoryQueue(shmkey, 10 * 2, queueModule, 1)

    # data = np.array([1, 2, 3, 4, 5 ,6, 7], dtype=np.uint8)  # Assuming you have some data to write
    sequence_array = np.arange(10, dtype=np.uint8)
    print("write data: ", sequence_array)
    print("len: ", len(sequence_array))
    for i in range(10):
        error_code = shmqueue.write(sequence_array, i)
        while error_code != 0:
            error_code = shmqueue.write(sequence_array, i)
    shmqueue.PrintTrunk()


    # print(shmqueue.getQueueLength())
# test()