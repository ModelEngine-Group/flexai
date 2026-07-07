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
        self.Threshold = 600
        self.scheduler_time_interval = 10
        self.stop_event = threading.Event()
        self.logging_level = 0 #0:INFO, 1:DEBUG
        self.policy = 1#0:FIFO, 1:SJF 2:SRTF 3:
        #FIFO
        self.fifo_queue = []
        self.fifo_queue_lock = threading.Lock()
        #SJF
        self.sjf_queue = []
        self.sjf_queue_lock = threading.Lock()
        #SRTF
        self.srtf_queue = []
        self.srtf_queue_lock = threading.Lock()
        #Themis
        self.themis_queue = []
        self.themis_queue_lock = threading.Lock()
        self.finish_flag = 0
        self.shared_queue = []
        self.exclusive_queue = []
    
    
    
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
                    logging.info(f"gpu:{gpu}")
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
                    if job not in self.is_running_list:
                        self.is_running_list.append(job)
                    if job in self.fifo_queue:
                        self.fifo_queue.remove(job)
                    if job in self.sjf_queue:
                        self.sjf_queue.remove(job)
                    node.job_run_list.append(job)
                    job.node_list.append(node)
                    logging.info(f'FIFO Job {job.clientID} is running on node {node.id}')
                    job.is_running = True
                    job.ready_replay = True
                    job.reallocated_flag = True
                    job.waiting_round = 0
                    break
    
    
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
            if node.has_free_gpu(4):
                return True
        return False    
    
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

            self.all_job_list.append(job)
            self.waiting_job_list.append(job)
            self.fifo_queue.append(job)
            self.sjf_queue.append(job)
            self.srtf_queue.append(job)
            self.themis_queue.append(job)
        client_gpu_list = []
        while job.ready_replay == False:
            time.sleep(1)
        # self.msg_server.redis_conn.publish(self.msg_server.clients[client_id]["status_channel"], "ALLOCATED")
        # if job.reallocated_flag == True:
        #     client_gpu_list = job.gpu_ids.copy()
        # else:
        #     client_gpu_list = []
        # print(client_gpu_list)
        client_gpu_list = job.gpu_ids.copy()
        self.msg_server.send_to_client(client_gpu_list, client_id_)
        if self.logging_level == 1:
            self.print_job_all_info(job)
            self.print_scheduler_info()
        job.ready_replay = False
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
        if cur_job.reallocated_flag == True:
            client_gpu_list = cur_job.gpu_ids.copy()
        else:
            client_gpu_list = []
        self.msg_server.send_to_client(client_gpu_list, client_id_)
        cur_job.ready_replay = False
        cur_job.reallocated_flag = False
        
    def print_job_all_info(self, job):
        logging.info("====================print job all info====================")
        print(job)
        if job in self.all_job_list:
            logging.info(f"{job.clientID} job in all_job_list")
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
            if data.startswith('TypeA:'):
                # self.To_Profiling(sender_id, data)
                logging.info(f"Server processed from {sender_id}: {data}")
                self.To_Scheduling(sender_id, data)
            elif data.startswith('TypeD:'):
                logging.info(f"Server processed from {sender_id}: {data}")
                self.Realloc_Resource(sender_id, data)
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
            if job.clientID == client_id:
                job.ServerConn = conn
                break
    
    def Job_Finish(self, conn, addr, message):
        data = message.split(":", 1)[1].split(',')
        client_id = int(data[0].strip())
        replay_flag = int(data[1].strip())
        for job in self.all_job_list:    
            if job.clientID == client_id:
                cur_job = job
                break
        if replay_flag == 1 and cur_job not in self.preemt_job_list:#说明任务没有被抢占，而是正常结束
            with self.is_running_list_lock:
                if cur_job in self.is_running_list:
                    self.is_running_list.remove(cur_job)
            with self.fifo_queue_lock:
                if cur_job in self.fifo_queue:
                    self.fifo_queue.remove(cur_job)
            with self.sjf_queue_lock:
                if cur_job in self.sjf_queue:
                    self.sjf_queue.remove(cur_job)
            with self.all_job_list_lock:
                if cur_job in self.all_job_list:
                    self.all_job_list.remove(cur_job)
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
        elif replay_flag == 1 and cur_job in self.preemt_job_list:#说明任务被抢占，需要重新分配资源
            logging.info(f"Job {cur_job.clientID} preempted")
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
                self.Job_Finish(conn, addr, message)
            if self.stop_event.wait(timeout=0.1):
                break
        conn.close()
        logging.info(f"Connection {addr} closed")

    def asyn_scheduler(self):
        logging.info("Sota Scheduler started......")
        self.scheduler_running = True
        while self.scheduler_running:
            if self.policy == 0:#FIFO
                logging.info("FIFO Scheduler started......")
                for i in self.waiting_job_list:
                    i.is_waiting_time += self.scheduler_time_interval
                    logging.info(f"job{i.clientID} in waiting_job_list, job waiting time:{i.is_waiting_time}")
                for i in self.is_running_list:
                    i.run_time += self.scheduler_time_interval
                    logging.info(f"job {i.clientID} run time:{i.run_time} reqgpu:{i.ReqGpuNum}")
                    
                with self.fifo_queue_lock:
                    tmp_fifo_list = self.fifo_queue.copy()
                tmp_fifo_list.sort(key=lambda x: x.arrival_time)
                logging.info(f"tmp_fifo_list:{len(tmp_fifo_list)}")
                
                for job in tmp_fifo_list:
                    logging.info(f"job:{job.clientID}")
                    self.try_allocate_resource(job)

            elif self.policy == 1:#SJF
                logging.info("SJF Scheduler started......")
                for i in self.waiting_job_list:
                    i.is_waiting_time += self.scheduler_time_interval
                    logging.info(f"job{i.clientID} in waiting_job_list, job waiting time:{i.is_waiting_time}")
                logging.info("=======run job info=======")
                for i in self.is_running_list:
                    i.run_time += self.scheduler_time_interval
                    logging.info(f"job {i.clientID} run time:{i.run_time} gpu:{i.gpu_ids[0].IP_addr}:{i.gpu_ids[0].gpu_id}")
                logging.info("=======gpu info=======")
                for gpu in self.node_list[0].gpu_list:
                    logging.info(f"gpu:{gpu.IP_addr}:{gpu.gpu_id} job_num:{gpu.Job_num}")
                    for job in gpu.job_list:
                        logging.info(f"job:{job.clientID}")
                
                with self.sjf_queue_lock:
                    tmp_sjf_list = self.sjf_queue.copy()
                tmp_sjf_list.sort(key=lambda x: x.predict_time)
                logging.info(f"tmp_sjf_list:{len(tmp_sjf_list)}")
                
                for job in tmp_sjf_list:
                    logging.info(f"sjf job:{job.clientID}")
                    self.try_allocate_resource(job)
            elif self.policy == 2:#SRTF
                logging.info("SRTF Scheduler started......")
                with self.waiting_job_list_lock:
                    tmp_waiting_list = self.waiting_job_list.copy()
                tmp_waiting_list.sort(key=lambda x: x.predict_time)
                for job in tmp_waiting_list:
                    if self.has_free_resource():
                        for node in self.node_list:
                            if node.free_gpu_num(0) >= job.ReqGpuNum:
                                tmp_gpu_num = job.ReqGpuNum
                                tmp_gpu_list = node.gpu_list.copy()
                                for gpu in tmp_gpu_list:
                                    if tmp_gpu_num == 0:
                                        break
                                    if gpu.gpu_is_free(3):
                                        gpu.update_gpu_info(job, 0)
                                        job.gpu_ids.append(gpu)
                                        tmp_gpu_num -= 1
                                node.job_run_list.append(job)
                                with self.waiting_job_list_lock:
                                    if job in self.waiting_job_list:
                                        self.waiting_job_list.remove(job)
                                with self.srtf_queue_lock:
                                    if job in self.srtf_queue:
                                        self.srtf_queue.remove(job)
                                with self.is_running_list_lock:
                                    self.is_running_list.append(job)
                                job.is_running = True
                                job.reallocated_flag = True
                                job.ready_replay = True
                                node.job_run_list.append(job)
                                job.node_list.append(node)
                                logging.info(f'SRTF Job {job.clientID} is running on node {node.id}')
                                break
                    else:#没有空闲资源了，抢占最大的任务
                        tmp_running_list = self.is_running_list.copy()
                        tmp_running_list.sort(key=lambda x: x.predict_time, reverse = True)
                        preemt_flag = False
                        while len(tmp_waiting_list) > 0 and not preemt_flag:
                            for cur_job in tmp_running_list:
                                if cur_job.predict_time > job.predict_time:
                                    with self.preemt_job_list_lock:
                                        self.preemt_job_list.append(cur_job)
                                    with self.is_running_list_lock:
                                        self.is_running_list.remove(cur_job)
                                        self.is_running_list.append(job)
                                    with self.waiting_job_list_lock:
                                        self.waiting_job_list.remove(job)
                                        self.waiting_job_list.append(cur_job)
                                    for gpu in cur_job.gpu_ids:
                                        gpu.update_gpu_info(cur_job, 1)
                                        gpu.update_gpu_info(job, 0)
                                        job.gpu_ids.append(gpu)
                                    for node in cur_job.node_list:
                                        node.job_run_list.remove(cur_job)
                                        node.job_run_list.append(job)
                                        job.node_list.append(node)
                                    cur_job.is_running = False
                                    cur_job.node_list.clear()
                                    cur_job.gpu_ids.clear()
                                    cur_job.ServerConn.send(b"stop")
                                    job.is_running = True
                                    job.reallocated_flag = True
                                    job.ready_replay = True
                                    logging.info(f'job {job.clientID} preempt {cur_job.clientID}')
                                    break    
                                else:
                                    preemt_flag = True
                                    break
            elif self.policy == 3:#Themis
                logging.info("Themis Scheduler started......")
                self.finish_flag = 0
                tmp_count = 0
                if(len(self.is_running_list)>0):
                    tmp_running_list = self.is_running_list.copy()
                    for job in tmp_running_list:
                        if job.is_exclusive == True:
                            job.exclusive_time += self.scheduler_time_interval
                            job.run_time += self.scheduler_time_interval
                        else :
                            job.shared_time += self.scheduler_time_interval
                            job.run_time += self.scheduler_time_interval * 0.5
                        if job.run_time > job.predict_time * 0.8:
                            continue
                        job.ServerConn.send(b"stop")
                        tmp_count += 1
                        job.is_running = False
                        job.node_list.clear()
                        job.gpu_ids.clear()
                        self.preemt_job_list.append(job)
                        self.is_running_list.remove(job)
                while self.finish_flag < tmp_count:
                    time.sleep(1)    
                free_gpu_num = 0
                for node in self.node_list:
                    free_gpu_num += node.has_free_gpu_num(3)
                if free_gpu_num > 1:
                    exclusive_gpu_num = free_gpu_num // 2
                    shared_gpu_num = free_gpu_num - exclusive_gpu_num
                    tmp_themis_list = self.themis_queue.copy()
                    tmp_themis_list.sort(key=lambda x: (-x.exclu_shared_ratio, x.arrival_time))
                    
                
            if self.stop_event.wait(timeout=self.scheduler_time_interval):
                break
        logging.info("aysn Scheduler stopped......")
        
        

def main():
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
        scheduler_thread.join()  # 等待线程结束
        logging.info("Server stopped")
        
    
    
        
if __name__ == "__main__":
    main()