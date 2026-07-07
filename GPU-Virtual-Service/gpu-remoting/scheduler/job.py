import argparse
import socket
import threading
import json
import redis
from ctypes import Structure, c_char, c_int, c_size_t, sizeof
from cffi import FFI
import struct
import logging
from collections import deque
import time
from multiprocessing import Process
import os 
import sys

current_dir = os.path.dirname(os.path.abspath(__file__))
parent_dir = os.path.dirname(current_dir)
sys.path.append(parent_dir)

from scheduler.gpu_info import *


logging.basicConfig(level=logging.INFO, format='%(asctime)s [%(levelname)s] %(message)s', datefmt='%Y-%m-%d %H:%M:%S')


#================Job_info================
class Job:
    def __init__(self, clientID, ReqGpuNum, priority, model, type, batchsize, gpu_Mem, gpu_util, runtime, conn, addr):
        self.clientID = clientID
        self.ReqGpuNum = ReqGpuNum
        self.priority = priority
        self.model = model
        self.type = type
        self.batchsize = batchsize
        self.isDDp = self.ReqGpuNum > 1

        #连接
        self.ClientConn = conn
        self.ServerConn = None
        self.addr = addr
        #任务信息
        if  ReqGpuNum == 1:
            self.gpu_mem = gpu_Mem 
        else:
            self.gpu_mem = gpu_Mem * 0.8
        self.gpu_util = gpu_util
        self.predict_time = runtime
        self.gpu_ids = []
        self.node_list = []
        #time
        self.arrival_time = time.time()
        self.is_waiting_time = 0
        self.run_time = 0
        self.job_served_time = 0
        self.last_update_time = time.time()
        self.preempted_time = 0
        self.is_running = False
        #replay
        self.allocate_flag = False
        self.ready_replay = False
        self.reallocated_flag = False
        self.waiting_round = 0
        #Themis
        self.exclusive_time = 1
        self.shared_time = 1
        self.is_exclusive = False
        #Lucid
        self.packing_type = -1 # 0:Tiny, 1:Medium, 2:Jumbo
    @property
    def exclu_shared_ratio(self):
        if self.exclusive_time == 0:
            return float('inf')
        return self.shared_time / self.exclusive_time    
    
    def update_job_info(self, jobPruntime, gpuMem, gpuUtil, priority):
        self.predict_time = jobPruntime
        self.gpu_mem = gpuMem   
        self.gpu_util = gpuUtil
        self.priority = priority
        self.arrival_time = time.time()
        self.last_update_time = time.time()
    
    @property
    def job_served(self):
        return self.job_served_time * self.ReqGpuNum
    
    @property
    def lucid_priority(self):#Temporal & Spatial Priority
        return self.ReqGpuNum * self.predict_time
    
    def __str__(self):
        return (f"Job(clientID={self.clientID}, ReqGpuNum={self.ReqGpuNum}, priority={self.priority}, "
                f"model={self.model}, type={self.type}, batchsize={self.batchsize}, gpu_mem={self.gpu_mem}, "
                f"gpu_util={self.gpu_util}, run_time={self.run_time}, gpu_ids={self.gpu_ids}, "
                f"node_list={self.node_list}, arrival_time={self.arrival_time}, predict_time={self.predict_time}, "
                f"is_waiting_time={self.is_waiting_time}, last_update_time={self.last_update_time}, "
                f"preempted_time={self.preempted_time}, is_running={self.is_running}, ready_replay={self.ready_replay}, "
                f"reallocated_flag={self.reallocated_flag}, waiting_round={self.waiting_round})")