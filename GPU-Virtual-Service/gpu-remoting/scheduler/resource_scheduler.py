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
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime

# 将 scheduler 目录添加到 sys.path 中
current_dir = os.path.dirname(os.path.abspath(__file__))
parent_dir = os.path.dirname(current_dir)
sys.path.append(parent_dir)

#自定义类
from scheduler.util import *
from scheduler.gpu_info import*
from scheduler.job import *
from scheduler.node import *
from scheduler.msg_queue import *

logging.basicConfig(level=logging.INFO, format='%(asctime)s [%(levelname)s] %(message)s', datefmt='%Y-%m-%d %H:%M:%S')



class Scheduler:
    def __init__(self):
        # self.cluster_list = []
        # self.cluster_list_lock = threading.Lock()
        self.node_list = []
        self.node_list_lock = threading.Lock()
        self.all_job_list = []
        self.all_job_list_lock = threading.Lock()
        self.allocated_list = []
        self.preemt_job_list = []
        self.preemt_job_list_lock = threading.Lock()
        self.is_running_list = []
        self.is_running_list_lock = threading.Lock()
        self.waiting_job_list = []
        self.waiting_job_list_lock = threading.Lock()
        self.msg_server = MSG_Server()
        self.executor = ThreadPoolExecutor(max_workers=5)
        self.scheduler_running = False
        #scheduler_Queue
        self.highpriority_queue = []
        self.highpriority_queue_lock = threading.Lock()
        self.lowpriority_queue = []
        self.lowpriority_queue_lock = threading.Lock()
        self.Threshold = 400
        self.scheduler_time_interval = 20
        self.stop_event = threading.Event()
        self.logging_level = 0 #0:INFO, 1:DEBUG
        self.finish_flag = 0

    def print_gpu_info(self):
        for node in self.node_list:
            for gpu in node.gpu_list:
                logging.info(f"GPU {gpu.IP_addr}:{gpu.gpu_id} memory:{gpu.memory_free} util:{gpu.utilization}")
    
    
    
    def sort_node_list(self, type, free_gpu_type, rev):
        if type == 1:#按照资源最空闲的节点排序
            if rev:
                self.node_list.sort(key=lambda x: x.free_gpu_num(free_gpu_type), reverse = True)
            else:
                self.node_list.sort(key=lambda x: x.free_gpu_num(free_gpu_type))
        elif type == 2:#按照job的数量排序
            if rev:
                self.node_list.sort(key=lambda x: x.total_job_num, reverse = True)
            else:
                self.node_list.sort(key=lambda x: x.total_job_num)
            
    def has_free_resource(self):
        for node in self.node_list:
            if node.has_free_gpu(0):
                return True
        return False    
    
    def clean_client_job(self, client_id):#client自己出错的情况，清理所有client资源
        cur_job = None
        for job in self.all_job_list:
            if job.clientID == client_id:
                cur_job = job
                break
        if cur_job == None:
            return
        if cur_job in self.all_job_list:
            self.all_job_list.remove(cur_job)
        if cur_job in self.highpriority_queue:
            self.highpriority_queue.remove(cur_job)
        if cur_job in self.lowpriority_queue:
            self.lowpriority_queue.remove(cur_job)
        if cur_job in self.preemt_job_list:
            self.preemt_job_list.remove(cur_job)
        if cur_job in self.is_running_list:
            self.is_running_list.remove(cur_job)
        if cur_job in self.waiting_job_list:
            self.waiting_job_list.remove(cur_job)
        for gpu in cur_job.gpu_ids:
            gpu.update_gpu_info(cur_job, 1)
        for node in cur_job.node_list:
            if cur_job in node.job_run_list:
                node.job_run_list.remove(cur_job)
        logging.info(f"job {cur_job.clientID} finished")
            
    
    def try_allocate_resource(self, job):
        tmp_node_list = self.node_list.copy()
        tmp_node_list.sort(key=lambda x: x.free_gpu_num(4)) 
        tmp_gpu_allocated_list = []
        for node in tmp_node_list:
            if node.free_gpu_num(4) >= job.ReqGpuNum:#有可能资源足够，已经是最空闲的节点
                tmp_job_gpu_num = job.ReqGpuNum
                tmp_gpu_list = node.gpu_list.copy()
                tmp_gpu_list.sort(key=lambda x: x.memory_free, reverse = True)
                for gpu in tmp_gpu_list:
                    try_allocate_flag = gpu.try_to_allocate(job)
                    if try_allocate_flag == False:
                        continue
                    tmp_gpu_allocated_list.append(gpu)
                    tmp_job_gpu_num -= 1
                    if tmp_job_gpu_num == 0:
                        job.allocate_flag = True
                        break
                if job.allocate_flag == True:
                    for gpu in tmp_gpu_allocated_list:
                        gpu.update_gpu_info(job, 0)
                        job.gpu_ids.append(gpu)
                    if job in self.waiting_job_list:
                        self.waiting_job_list.remove(job)
                    self.is_running_list.append(job)
                    if job not in self.highpriority_queue:
                        self.highpriority_queue.append(job)
                    node.job_run_list.append(job)
                    job.node_list.append(node)
                    job.is_running = True
                    job.ready_replay = True
                    job.reallocated_flag = True
                    break
    
    def try_reallocate_resource(self, job):
        tmp_node_list = self.node_list.copy()
        tmp_node_list.sort(key=lambda x: x.free_gpu_num(4)) 
        tmp_gpu_allocated_list = []
        job.gpu_ids.clear()
        job.node_list.clear()
        for node in tmp_node_list:
            if node.free_gpu_num(4) >= job.ReqGpuNum:#有可能资源足够，已经是最空闲的节点
                tmp_job_gpu_num = job.ReqGpuNum
                tmp_gpu_list = node.gpu_list.copy()
                tmp_gpu_list.sort(key=lambda x: x.memory_free, reverse = True)
                for gpu in tmp_gpu_list:
                    try_allocate_flag = gpu.try_to_allocate(job)
                    if try_allocate_flag == False:
                        continue
                    tmp_gpu_allocated_list.append(gpu)
                    tmp_job_gpu_num -= 1
                    if tmp_job_gpu_num == 0:
                        job.allocate_flag = True
                        break
                if job.allocate_flag == True:
                    for gpu in tmp_gpu_allocated_list:
                        gpu.update_gpu_info(job, 0)
                        job.gpu_ids.append(gpu)
                    if job in self.preemt_job_list:
                        self.preemt_job_list.remove(job)
                    self.is_running_list.append(job)
                    node.job_run_list.append(job)
                    job.node_list.append(node)
                    job.is_running = True
                    job.ready_replay = True
                    job.reallocated_flag = True
                    job.waiting_round = 0
                    break
    
    def try_preempt_resource(self, job):
        logging.info(f"job{job.clientID}try to preempted_resource")
        tmp_record = []
        for tmp_job in self.is_running_list:
            if tmp_job in self.lowpriority_queue and tmp_job.isDDp == False:
                tmp_record.append(tmp_job)  
        if len(tmp_record) == 0:
            return 
        tmp_record.sort(key=lambda x: x.job_served, reverse = True)
        gpu_list = []
        task = {}
        m = job.gpu_mem
        k = job.ReqGpuNum
        for tmp_job_2 in tmp_record:
            gpu_id = tmp_job_2.gpu_ids[0].gpu_id
            gpu_ip = tmp_job_2.gpu_ids[0].IP_addr
            gpu = f"{gpu_ip}:{gpu_id}"
            if gpu not in gpu_list:
                gpu_list.append(gpu)
            id = tmp_job_2.clientID
            job_mem = tmp_job_2.gpu_mem
            job_served = tmp_job_2.job_served
            task[id] = (gpu, job_mem, job_served)
        logging.info(f"gpu_list:{gpu_list}")
        logging.info(f"task:{task}")
        logging.info(f"m:{m} k:{k}")
        result = optimize_task_preemption(gpu_list, task, m, k)
        job.gpu_ids.clear()
        job.node_list.clear()
        if result["status"] == "Optimal":#抢占成功
            logging.info(f"job{job.clientID}preempted success......")
            tmp_count = len(result["preempted_tasks"])
            self.finish_flag = 0
            for p_job in result["preempted_tasks"]:
                pree_job_id = p_job["task_id"]
                for i in self.all_job_list:
                    if i.clientID == pree_job_id:
                        if i.gpu_ids[0] not in job.gpu_ids:
                            job.gpu_ids.append(gpu)    
                        gpu = i.gpu_ids[0]
                        gpu.update_gpu_info(i, 1)
                        gpu.update_gpu_info(job, 0)
                        i.gpu_ids.clear()
                        i.node_list.clear()
                        i.job_served_time = 0
                        self.preemt_job_list.append(i)
                        if i in self.is_running_list:
                            self.is_running_list.remove(i)
                        i.ServerConn.sendall(b"stop")
                        break
            job.allocate_flag = True
            while self.finish_flag < tmp_count:
                time.sleep(1)
            job.is_running = True
            job.ready_replay = True
            job.reallocated_flag = True
            if job in self.waiting_job_list:
                self.waiting_job_list.remove(job)
            self.is_running_list.append(job)
            job.waiting_round = 0
        else:
            return
            
                 
    def To_Profiling(self, client_id, message):
        message = message.split(":", 1)[1].split(',') # 获取 "clientID,gpuCount"
        client_id_ = int(message[0].strip())
        req_gpu_num = int(message[1].strip().replace('\x00', ''))
        with self.all_job_list_lock:
            job = Job(client_id_, req_gpu_num, 0, 0, 0, 0, 0, 0, 0, None, '')
            self.all_job_list.append(job)
        config = get_config_file()
        r = redis_connection()
        prof_server_ip = config['ProfilingConfig']['profilingIp_']
        redis_key = f'{prof_server_ip}:{0}'#192.168.0.209:0
        gpu_info_pro = r.hgetall(redis_key)
        gpu_info = GPU_info(gpu_info_pro)
        tmp_gpu_list = []
        tmp_gpu_list.append(gpu_info)
        self.msg_server.redis_conn.publish(self.msg_server.clients[client_id]["status_channel"], "ALLOCATED")
        self.msg_server.send_to_client(tmp_gpu_list, client_id)
        
        
    def To_Scheduling(self, client_id, data):
        message = data.split(":", 1)[1].split(',') # 获取 "clientID,gpuCount"
        client_id_ = int(message[0].strip())
        req_gpu_num = int(message[1].strip().replace('\x00', ''))
        model = message[2].strip()
        batch_size = int(message[3].strip().replace('\x00', ''))

        result = query_job_info(model, batch_size)  
        gpu_mem = result.iloc[0]['gpu_mem']
        gpu_util = result.iloc[0]['gpu_util']
        pre_run_time = result.iloc[0]['runtime']
        
        with self.all_job_list_lock:
            job = Job(client_id_, req_gpu_num, 0, model, 0, batch_size, gpu_mem, gpu_util, pre_run_time, None, '')
            if job not in self.all_job_list:
                self.all_job_list.append(job)
            if job not in self.waiting_job_list:
                self.waiting_job_list.append(job)
            if job not in self.highpriority_queue:
                self.highpriority_queue.append(job)
        client_gpu_list = []
        
        while job.ready_replay == False:
            time.sleep(1)
        # self.msg_server.redis_conn.publish(self.msg_server.clients[client_id]["status_channel"], "ALLOCATED")
        # if job.allocate_flag == True:
        #     client_gpu_list = job.gpu_ids.copy()
        # else:
        #     client_gpu_list = []
        client_gpu_list = job.gpu_ids.copy()
        # print(client_gpu_list)
        self.msg_server.send_to_client(client_gpu_list, client_id_)
        if self.logging_level == 1:
            self.print_job_all_info(job)
            self.print_scheduler_info()
        job.ready_replay = False
        job.allocate_flag = False
        job.reallocated_flag = False
        
    def Realloc_Resource(self, client_id, message):
        data = message.split(":", 1)[1].split(',')
        client_id_ = int(data[0].strip())
        req_gpu_num = int(data[1].strip())
        for job in self.all_job_list:
            if job.clientID == client_id_:
                cur_job = job
                break
        while cur_job.ready_replay == False:
            time.sleep(1)
        # self.msg_server.redis_conn.publish(self.msg_server.clients[client_id]["status_channel"], "ALLOCATED")
        # if cur_job.reallocated_flag == True:
        #     client_gpu_list = cur_job.gpu_ids.copy()
        # else:
        #     client_gpu_list = []
        client_gpu_list = job.gpu_ids.copy()
        self.msg_server.send_to_client(client_gpu_list, client_id_)
        cur_job.ready_replay = False
        cur_job.reallocated_flag = False
        cur_job.allocate_flag = False
        
    def print_job_all_info(self, job):
        logging.info("====================print job all info====================")
        print(job)
        if job in self.all_job_list:
            logging.info(f"{job.clientID} job in all_job_list")
        if job in self.is_running_list:
            logging.info(f'{job.clientID} job in is_running_list')
        if job in self.highpriority_queue:
            logging.info(f'{job.clientID} job in highpriority_queue')
        if job in self.lowpriority_queue:
            logging.info(f'{job.clientID} job in lowpriority_queue')
        if job in self.preemt_job_list:
            logging.info(f'{job.clientID} job in preemt_job_list')
        if job in self.waiting_job_list:
            logging.info(f'{job.clientID} job in waiting_job_list')
        for node in job.node_list:
            logging.info(f"node:{node.id}")    
        for gpu in job.gpu_ids:
            logging.info(f"gpu:{gpu.IP_addr}:{gpu.gpu_id}")
        print("========================================================================")
        
    def print_scheduler_info(self):
        logging.info("====================print scheduler info====================")
        for node in self.node_list:
            logging.info(f"node:{node.id}:")
            for job in node.job_run_list:
                logging.info(f"job:{job.clientID}")
            for gpu in node.gpu_list:
                logging.info(gpu)
                for job in gpu.job_list:
                    logging.info("====================================")
                    logging.info(job)
                    logging.info("====================================")
                    
        logging.info("all_job_list:")
        for job in self.all_job_list:
            logging.info(f"job:{job.clientID}")
        logging.info("is_running_list:")
        for job in self.is_running_list:
            logging.info(f"job:{job.clientID}")
        logging.info("highpriority_queue:")
        for job in self.highpriority_queue:
            logging.info(f"job:{job.clientID}")
        logging.info("lowpriority_queue:")
        for job in self.lowpriority_queue:
            logging.info(f"job:{job.clientID}")
        logging.info("preemt_job_list:")
        for job in self.preemt_job_list:
            logging.info(f"job:{job.clientID}")
        logging.info("waiting_job_list:")
        for job in self.waiting_job_list:
            logging.info(f"job:{job.clientID}")
        logging.info("========================================================================")
    
    def process_message(self, message):
        # 处理单条消息的逻辑（线程安全）
        channel = message['channel'].decode()
        data = message['data'].decode()
        if channel == REGISTER_CHANNEL:
            self.msg_server.register_client(data)
        else:
            sender_id = channel.split("_")[-1]
            sender_id = int(sender_id)
            if data.startswith('TypeA:'):
                # self.To_Profiling(sender_id, data)
                logging.info(f"Type A :Server processed from {sender_id}: {data}")
                self.To_Scheduling(sender_id, data)
            elif data.startswith('TypeD:'):
                logging.info(f"Server processed from {sender_id}: {data}")
                self.Realloc_Resource(sender_id, data)
            elif data == 'STOP':
                self.clean_client_job(sender_id)
                self.msg_server.stop_client(sender_id)
            # print(f"Server processed from {sender_id}: {data}")
        
    def recieve_message_queue(self):#只处理来自proxy的消息
        logging.info("MSG_Server started, listening for registrations and messages")
        while self.msg_server.running:
            message = self.msg_server.pubsub.get_message(timeout=1.0)
            if message and message['type'] == 'message':
                self.executor.submit(self.process_message, message)
            time.sleep(0.01)

    def Finish_Profiling(self, conn, addr, message):
        data = message.split(":", 1)[1].split(',')
        client_id = int(data[0].strip())
        req_gpu_num = int(data[1].strip())
        job_runtime = float(data[2].strip())
        gpu_mem = int(data[3].strip())
        gpu_util = int(data[4].strip())
        job_priority = int(data[5].strip())
        for job in self.all_job_list:
            if job.clientID == client_id:
                job.update_job_info(job_runtime, gpu_mem, gpu_util, job_priority)
                break
        self.waiting_job_list.append(job)
        
    def Add_Job_ServerConn(self, conn, addr, message):
        data = message.split(":", 1)[1].split(',')
        client_id = int(data[0].strip())
        logging.info(f"client_id:{client_id} server connection established")
        for job in self.all_job_list:
            if job.clientID == client_id and job.ServerConn ==None:
                job.ServerConn = conn
                break
    
    def Job_Finish(self, conn, addr, message):
        data = message.split(":", 1)[1].split(',')
        client_id = int(data[0].strip())
        replay_flag = int(data[1].strip())
        cur_job = None
        for job in self.all_job_list:    
            if job.clientID == client_id:
                cur_job = job
                break
        if cur_job == None:
            return
        if replay_flag == 1 and cur_job not in self.preemt_job_list:#说明任务没有被抢占，而是正常结束
            with self.is_running_list_lock:
                if cur_job in self.is_running_list:
                    self.is_running_list.remove(cur_job)
            with self.highpriority_queue_lock:
                if cur_job in self.highpriority_queue:
                    self.highpriority_queue.remove(cur_job)
            with self.lowpriority_queue_lock:
                if cur_job in self.lowpriority_queue:
                    self.lowpriority_queue.remove(cur_job)
            with self.all_job_list_lock:
                if cur_job in self.all_job_list:
                    self.all_job_list.remove(cur_job)
            with self.waiting_job_list_lock:
                if cur_job in self.waiting_job_list:
                    self.waiting_job_list.remove(cur_job)
            with self.preemt_job_list_lock:
                if cur_job in self.preemt_job_list:
                    self.preemt_job_list.remove(cur_job)
            for gpu in cur_job.gpu_ids:
                gpu.update_gpu_info(cur_job, 1)
            for node in cur_job.node_list:
                node.job_run_list.remove(cur_job)
            r = redis_job_connection()
            job_info = {
                "job_id": int(cur_job.clientID),
                "job_req_gpu_num": int(cur_job.ReqGpuNum),
                "job_model": cur_job.model,
                "job_batch_size": int(cur_job.batchsize),
                "job_gpu_mem": int(cur_job.gpu_mem),
                "job_gpu_util": float(cur_job.gpu_util),
                "job_runtime": float(cur_job.predict_time),
                "job_arrival_time": float(cur_job.arrival_time),
                "job_run_time": float(cur_job.run_time),
                "job_is_waiting_time": float(cur_job.is_waiting_time),
                "job_preempted_time": float(cur_job.preempted_time)
            }
            job_info_json = json.dumps(job_info)
            r.hset(f"{cur_job.clientID}", "job_info", job_info_json)
            
            
            logging.info(f"Job {cur_job.clientID} finished")
            logging.info("+++++++++++++++++++++++++++++++++++++++++++++++++++++")
            self.print_scheduler_info()
            
        elif replay_flag == 1 and cur_job in self.preemt_job_list:#说明任务被抢占，需要重新分配资源
            logging.info(f"Job {cur_job.clientID} preempted")
            if cur_job in self.is_running_list:
                self.is_running_list.remove(cur_job)
            if cur_job is not None:
                cur_job.ServerConn = None
            self.finish_flag += 1
            logging.info(f"finish_flag:{self.finish_flag}")
        elif replay_flag == 0:
            with self.is_running_list_lock:
                if cur_job in self.is_running_list:
                    self.all_job_list.remove(cur_job)
            with self.highpriority_queue_lock:
                if cur_job in self.highpriority_queue:
                    self.highpriority_queue.remove(cur_job)
            for gpu in cur_job.gpu_ids:
                gpu.update_gpu_info(cur_job, 1)
            for node in cur_job.node_list:
                node.job_run_list.remove(cur_job)
            logging.info(f"job {cur_job.clientID} finished")
            self.print_scheduler_info()
    
    def handle_message(self, conn, addr):
        while True:
            data = conn.recv(1024)
            if not data:
                break
            message = data.decode()
            if message.startswith('TypeB:'):
                self.Finish_Profiling(conn, addr, message)
            elif message.startswith('TypeC:'):
                self.Add_Job_ServerConn(conn, addr, message)
            elif message.startswith('TypeE:'):
                logging.info("handle_type_E message{}".format(message))
                self.Job_Finish(conn, addr, message)
            if self.stop_event.wait(timeout=0.1):
                break
        conn.close()
        logging.info(f"Connection {addr} closed")

    def asyn_scheduler(self):
        logging.info("Aysn Scheduler started......")
        self.scheduler_running = True
        while self.scheduler_running:
            logging.info("new round Scheduler running......")
            logging.info("------------------------------------------------------------------------")
            self.print_gpu_info()
            logging.info("------------------------------------------------------------------------")
            #1、先检查highpriority_queue中的作业是否已经达到了阈值,是的话先放入到lowpriority_queue中
            logging.info("========================================================================")
            logging.info("all job list len:%d",len(self.all_job_list))
            logging.info("highpriority queue len:%d",len(self.highpriority_queue))
            logging.info("lowpriority queue len:%d",len(self.lowpriority_queue))
            logging.info("preemt job list len:%d",len(self.preemt_job_list))
            logging.info("is running list len:%d",len(self.is_running_list))
            logging.info("waiting job list len:%d",len(self.waiting_job_list))
            logging.info("========================================================================")
            logging.info("all job list_infomation:")
            for it in self.all_job_list:
                logging.info(f"job {it.clientID} ReqGpuNum:{it.ReqGpuNum} is_running:{it.is_running} job_served:{it.job_served} is_waiting_time:{it.is_waiting_time} preempted_time:{it.preempted_time}")
            logging.info("===========================Running List:===================================")
            with self.is_running_list_lock:
                for job in self.is_running_list:
                    job.run_time += self.scheduler_time_interval
                    job.job_served_time += self.scheduler_time_interval
                    logging.info(f"job {job.clientID} run time:{job.run_time} reqgpu:{job.ReqGpuNum} job_served:{job.job_served}")
                    if job in self.highpriority_queue and job.job_served > self.Threshold:
                        self.highpriority_queue.remove(job)
                        self.lowpriority_queue.append(job)
            #查看被抢占作业是否已经达到了阈值,是的话重新放入到waiting_job_list中
            with self.preemt_job_list_lock:
                for job in self.preemt_job_list:
                    job.preempted_time += self.scheduler_time_interval
                    job.is_waiting_time += self.scheduler_time_interval
                    if job.preempted_time > self.Threshold * 0.5:
                        self.preemt_job_list.remove(job)
                        self.waiting_job_list.append(job) 
                        if job in self.lowpriority_queue:
                            self.lowpriority_queue.remove(job)
                        job.preempted_time = 0        
            #2、针对新作业，先检查是否有足够的资源，如果有的话，直接分配资源
            tmp_waiting_queue = self.waiting_job_list.copy()
            self.sort_node_list(1, 4, True)
            tmp_waiting_queue.sort(key=lambda x: x.arrival_time)
            print("waiting queue len:",len(tmp_waiting_queue))
            for job in tmp_waiting_queue:
                logging.info("waiting job clientID:%d",job.clientID)
                self.try_allocate_resource(job)
                # if job.is_running == True:
                #     self.print_scheduler_info()
            #3、检查是否有足够的资源，如果没有的话，检查是否有作业可以抢占资源
                if job.is_running == False and job.waiting_round >= 3:
                    self.try_preempt_resource(job)
                    if job.is_running == False:
                        job.is_waiting_time += self.scheduler_time_interval
                        job.waiting_round = job.is_waiting_time // 100
                        continue
                else:
                    job.is_waiting_time += self.scheduler_time_interval
                    job.waiting_round = job.is_waiting_time // 100
            # time.sleep(self.scheduler_time_interval)
            #4检查被抢占的任务列表是否可以被重新分配资源
            for job in self.preemt_job_list:
                logging.info(f"job{job.clientID} in preemt_job_list")
                self.try_reallocate_resource(job)
            if self.stop_event.wait(timeout=self.scheduler_time_interval):
                break
        logging.info("aysn Scheduler stopped......")
        
def setup_logging_and_output(log_path):
    # 清空所有现有的 handlers，避免默认输出到终端
    logging.getLogger().handlers = []
    
    # 创建文件 handler
    file_handler = logging.FileHandler(log_path)
    file_handler.setLevel(logging.INFO)
    formatter = logging.Formatter('%(asctime)s - %(levelname)s - %(message)s')
    file_handler.setFormatter(formatter)
    
    # 配置 root logger
    logger = logging.getLogger()
    logger.setLevel(logging.INFO)
    logger.addHandler(file_handler)
    
    # 重定向 sys.stdout 和 sys.stderr 到文件
    log_file = open(log_path, 'a', encoding='utf-8')  # 'a' 表示追加模式
    sys.stdout = log_file
    sys.stderr = log_file
    
    # 返回文件对象以便在程序结束时关闭
    return log_file

def main():
    #输出重定向
    redirect = 0
    if redirect == 1:
        # 获取当前文件所在目录
        current_dir = os.path.dirname(os.path.abspath(__file__))
        # 获取上一层目录
        parent_dir = os.path.dirname(current_dir)
        # 创建 result_log 文件夹路径
        result_log_dir = os.path.join(parent_dir, "20250306schduleresult_log")
        
        # 如果文件夹不存在，则创建
        if not os.path.exists(result_log_dir):
            os.makedirs(result_log_dir)
        
        # 生成带有时间戳的文件名并保存到 result_log 文件夹
        current_time = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
        log_filename = f"{current_time}_dispatcher.log"
        log_path = os.path.join(result_log_dir, log_filename)  # 完整日志文件路径
        
        # 设置日志和输出重定向
        log_file = setup_logging_and_output(log_path)
    
    
    config = get_config_file()
    disp_ip = config['DispatcherConfig']['dpcIp_']
    disp_port = config['DispatcherConfig']['dpcPort_']
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.bind((disp_ip, disp_port))
    s.listen()
    logging.info("Server started listen at %s:%d", disp_ip, disp_port)
    
    thread_list = []
    
    serverIp = config['ServerConfig']['serverIp_']
    scheduler = Scheduler()
    node_1 = Node(0, serverIp)
    scheduler.node_list.append(node_1)
    for gpu in node_1.gpu_list:
        logging.info(f"GPU {gpu.IP_addr}:{gpu.gpu_id} added...")
    
    
    msg_thread = threading.Thread(target=scheduler.recieve_message_queue)
    msg_thread.start()
    
    scheduler_thread = threading.Thread(target=scheduler.asyn_scheduler)
    scheduler_thread.start()
    
    
    try:
        while True:
            conn, addr = s.accept()
            logging.info("New connection from %s:%d", addr[0], addr[1])
            t = threading.Thread(target=scheduler.handle_message, args=(conn, addr))
            t.start()
            thread_list.append(t)
    except KeyboardInterrupt:
        scheduler.msg_server.stop()
        s.close()
        scheduler.stop_event.set()  # 设置停止事件
        scheduler.scheduler_running = False
        # scheduler_thread.join()  # 等待线程结束
        logging.info("Server stopped")
    
    if redirect == 1:
        log_file.close()
        # 恢复 sys.stdout 和 sys.stderr（可选，避免影响后续程序）
        sys.stdout = sys.__stdout__
        sys.stderr = sys.__stderr__
    
        
if __name__ == "__main__":
    main()