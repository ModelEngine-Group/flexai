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
from scheduler.node import *


logging.basicConfig(level=logging.INFO, format='%(asctime)s [%(levelname)s] %(message)s', datefmt='%Y-%m-%d %H:%M:%S')


class Cluster:
    def __init__(self):
        self.node_list = []
        self.node_list_lock = threading.Lock()
        self.all_job_list = []
        self.job_list_lock = threading.Lock()
        self.preemt_job_list = []
        self.preemt_job_list_lock = threading.Lock()
        self.running_job_list = []
        self.running_job_list_lock = threading.Lock()
        self.waiting_job_list = []
        self.waiting_job_list_lock = threading.Lock()
        
        
    def sort_node_list(self, type, free_gpu_type, rev):
        if type == 1:#按照资源最空闲的节点排序
            if rev:
                self.node_list.sort(key=lambda x: x.free_gpu_num(free_gpu_type), reverse=True)
            else:
                self.node_list.sort(key=lambda x: x.free_gpu_num(free_gpu_type))
        elif type == 2:#按照job的数量排序
            if rev:
                self.node_list.sort(key=lambda x: x.total_job_num, reverse = True)
            else:
                self.node_list.sort(key=lambda x: x.total_job_num)