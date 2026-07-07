
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

from scheduler.util import *
from scheduler.job import *



logging.basicConfig(level=logging.INFO, format='%(asctime)s [%(levelname)s] %(message)s', datefmt='%Y-%m-%d %H:%M:%S')


class GPU_info:
    def __init__(self, gpu_info_properties):
        self.gpu_info_properties = gpu_info_properties
        gpu_infos = json.loads(gpu_info_properties[b'gpu_info'])
        self.gpu_id = gpu_infos['gpu_id']
        self.IP_addr = gpu_infos['IP_addr']
        self.Port = gpu_infos['Port']
        self.memory_total = gpu_infos['memory_total']
        self.memory_free = gpu_infos['memory_free']
        self.memory_used = gpu_infos['memory_used']
        self.utilization = gpu_infos['utilization']
        self.Job_num = gpu_infos['Job_num']
        self.GPU_num = gpu_infos['GPU_num']
        self.HandlerIp = gpu_infos['HandlerIp']
        self.HandlerPort = gpu_infos['HandlerPort']
        self.gpu_properties = gpu_info_properties[b'gpu_properties']
        self.job_list = []
        self.has_high_priority = False

    def gpu_is_free(self, type = 0):
        if type == 0:
            return self.Job_num < 4
        elif type == 1:
            return self.memory_free > self.memory_total * (1 - 0.92)
        elif type == 2:
            return self.utilization < 90
        elif type == 3:
            return self.Job_num == 0
        elif type == 4:
            # return self.memory_used < self.memory_total * 0.92 and self.utilization < 95 and self.Job_num < 4
            return self.memory_used < self.memory_total * 0.92 and self.Job_num < 4
        elif type == 5:
            return self.memory_free > self.memory_total * 0.92 and self.Job_num < 2

    def update_gpu_info(self, job, type = 0):#type=0:添加该任务到GPU，type=1:从GPU中删除该任务
        if type == 0:
            self.memory_free -= job.gpu_mem
            self.memory_used += job.gpu_mem
            self.utilization += job.gpu_util
            self.Job_num += 1
            self.job_list.append(job)
        else:
            self.memory_free += job.gpu_mem
            self.memory_used -= job.gpu_mem
            self.utilization -= job.gpu_util
            self.Job_num -= 1
            if job in self.job_list:
                self.job_list.remove(job)

    def try_to_allocate(self, job):
        if self.memory_free > job.gpu_mem and self.utilization + job.gpu_util < 100 and self.Job_num < 4:
            return True
        return False
        
        
    def try_to_allocate_lucid(self, job, shared_policy):
        if shared_policy == 0:
            if self.memory_free > job.gpu_mem and self.utilization + job.gpu_util < 150 and self.Job_num == 0:
                return True
            else:
                return False
        else:
            if self.memory_free > job.gpu_mem and self.utilization + job.gpu_util < 150 and self.Job_num < 2:
                return True
            else:
                return False
            
    def __str__(self):
        return (f"gpu_id: {self.gpu_id}, IP_addr: {self.IP_addr}, Port: {self.Port}, memory_total: {self.memory_total}, "
                f"memory_free: {self.memory_free}, memory_used: {self.memory_used}, utilization: {self.utilization}, "
                f"Job_num: {self.Job_num}, GPU_num: {self.GPU_num}, HandlerIp: {self.HandlerIp}, HandlerPort: {self.HandlerPort}, "
                f"job_list: {self.job_list}, has_high_priority: {self.has_high_priority}")
        