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


# 将 scheduler 目录添加到 sys.path 中
current_dir = os.path.dirname(os.path.abspath(__file__))
parent_dir = os.path.dirname(current_dir)
sys.path.append(parent_dir)

#自定义类
from scheduler.util import *
from scheduler.gpu_info import*
from scheduler.job import *


logging.basicConfig(level=logging.INFO, format='%(asctime)s [%(levelname)s] %(message)s', datefmt='%Y-%m-%d %H:%M:%S')


class Node:
    def __init__(self, id, ip):
        self.id = id
        self.ip = ip
        self.job_run_list = []
        self.job_run_list_lock = threading.Lock()
        self.gpu_list = []
        
        r = redis_connection()
        config = get_config_file()
        self.gpu_num = config['ServerConfig']['serverGPU_']
        for i in range(self.gpu_num):
            redis_key = f"{self.ip}:{i}"
            gpu_info_pro = r.hgetall(redis_key)
            gpu = GPU_info(gpu_info_pro)
            self.gpu_list.append(gpu)
            print(gpu)
            
            
    def update_gpu_info(self, gpu_id, job, type):#type=0:分配内存，type=1:释放内存
        for gpu in self.gpu_list:
            if gpu.gpu_id == gpu_id:
                cur_gpu = gpu
        if cur_gpu != None:
            cur_gpu.update_gpu_info(job, type)
    
    def sort_gpu_list(self, type, rev):
        if type == 1:
            if rev:
                self.gpu_list.sort(key=lambda x: x.Job_num, reverse = True)
            else: 
                self.gpu_list.sort(key=lambda x: x.Job_num)
        elif type == 2:
            if rev:
                self.gpu_list.sort(key=lambda x: x.memory_free, reverse = True)
            else: 
                self.gpu_list.sort(key=lambda x: x.memory_free)
        elif type == 3:
            if rev:
                self.gpu_list.sort(key=lambda x: x.utilization, reverse = True)
            else: 
                self.gpu_list.sort(key=lambda x: x.utilization)
            
    def has_free_gpu(self, type):
        for gpu in self.gpu_list:
            if gpu.gpu_is_free(type):
                return True
        return False
            
    def sort_job_list(self, type):
        if type == 1:
            self.job_run_list.sort(key=lambda x: x.arrival_time)
        elif type == 2:
            self.job_run_list.sort(key=lambda x: x.runtime, reverse = True)
        elif type == 3:
            self.job_run_list.sort(key=lambda x: x.job_served, reverse = True)
            
        
    def free_gpu_num(self, type):
        count = 0
        for gpu in self.gpu_list:
            if gpu.gpu_is_free(type):
                count += 1
        return count
    
    def free_gpus(self, type):
        free_gpu_list = []
        for gpu in self.gpu_list:
            if gpu.gpu_is_free(type):
                free_gpu_list.append(gpu)
        return free_gpu_list
    
    @property
    def total_job_num(self):
        return len(self.job_run_list)
    
    def has_free_gpu_num(self, type):
        count = 0
        for gpu in self.gpu_list:
            if gpu.gpu_is_free(type):
                count += 1
        return count