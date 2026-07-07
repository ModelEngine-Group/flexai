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
        self.Threshold = 600
        self.scheduler_time_interval = 20
        self.stop_event = threading.Event()
        self.logging_level = 0 #0:INFO, 1:DEBUG

    
    
    
    
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
    
    def try_allocate_resource(self, job):
        tmp_node_list = self.node_list.copy()
        tmp_node_list.sort(key=lambda x: x.free_gpu_num(4)) 
        tmp_gpu_allocated_list = []
        for node in tmp_node_list:
            if node.free_gpu_num(4) >= job.ReqGpuNum:#有可能资源足够，已经是最空闲的节点
                tmp_job_gpu_num = job.ReqGpuNum
                tmp_gpu_list = node.gpu_list.copy()
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
                    self.waiting_job_list.remove(job)
                    self.is_running_list.append(job)
                    self.highpriority_queue.append(job)
                    node.job_run_list.append(job)
                    job.node_list.append(node)
                    job.is_running = True
                    job.ready_replay = True
                    break
        
    def try_preempt_resource(self, job):
        tmp_preemted_job_list = []
        tmp_job_gpu_list = []
        
                
    
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
        if job.allocate_flag == True:
            client_gpu_list = job.gpu_ids.copy()
        else:
            client_gpu_list = []
        # print(client_gpu_list)
        self.msg_server.send_to_client(client_gpu_list, client_id_)
        if self.logging_level == 1:
            self.print_job_all_info(job)
            self.print_scheduler_info()
        job.ready_replay = False
        job.allocate_flag = False
        
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
        if cur_job.realloacted_flag == True:
            client_gpu_list = cur_job.gpu_ids.copy()
        else:
            client_gpu_list = []
        self.msg_server.send_to_client(client_gpu_list, client_id_)
        cur_job.ready_replay = False
        cur_job.realloacted_flag = False
        
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
                logging.info(f"Type A :Server processed from {sender_id}: {data}")
                self.To_Scheduling(sender_id, data)
            elif data.startswith('TypeD:'):
                logging.info(f"Server processed from {sender_id}: {data}")
                self.Realloc_Resource(sender_id, data)
            elif data == 'STOP':
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
                    self.all_job_list.remove(cur_job)
            with self.highpriority_queue_lock:
                if cur_job in self.highpriority_queue:
                    self.highpriority_queue.remove(cur_job)
            with self.lowpriority_queue_lock:
                if cur_job in self.lowpriority_queue:
                    self.lowpriority_queue.remove(cur_job)
            with self.all_job_list_lock:
                if cur_job in self.all_job_list:
                    self.all_job_list.remove(cur_job)
            for gpu in cur_job.gpu_ids:
                gpu.update_gpu_info(cur_job, 1)
            for node in cur_job.node_list:
                node.job_run_list.remove(cur_job)
            logging.info(f"Job {cur_job.clientID} finished")
        elif replay_flag == 1 and cur_job in self.preemt_job_list:#说明任务被抢占，需要重新分配资源
            logging.info(f"Job {cur_job.clientID} preempted")
            if cur_job is not None:
                cur_job.ServerConn = None
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
                logging.info("handle_type_E message")
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
            #1、先检查highpriority_queue中的作业是否已经达到了阈值,是的话先放入到lowpriority_queue中
            with self.is_running_list_lock:
                for job in self.is_running_list:
                    job.run_time += self.scheduler_time_interval
                    if job in self.highpriority_queue and job.job_served > self.Threshold:
                        self.highpriority_queue.remove(job)
                        self.lowpriority_queue.append(job)
            #查看被抢占作业是否已经达到了阈值,是的话重新放入到waiting_job_list中
            with self.preemt_job_list_lock:
                for job in self.preemt_job_list:
                    job.preempted_time += self.scheduler_time_interval
                    if job.preempted_time > self.Threshold * 0.5:
                        self.preemt_job_list.remove(job)
                        self.waiting_job_list.append(job)           
            #2、针对新作业，先检查是否有足够的资源，如果有的话，直接分配资源
            tmp_waiting_queue = self.waiting_job_list.copy()
            self.sort_node_list(1, 4, True)
            tmp_waiting_queue.sort(key=lambda x: x.arrival_time)
            print("waiting queue len:",len(tmp_waiting_queue))
            for job in tmp_waiting_queue:
                self.try_allocate_resource(job)
            #3、检查是否有足够的资源，如果没有的话，检查是否有作业可以抢占资源
                if job.is_running == False and job.waiting_round >= 2:#没有足够的资源,抢占资源
                    tmp_running_job_list = self.is_running_list.copy()
                    tmp_running_job_list.sort(key=lambda x: x.job_served, reverse = True)
                    get_gpu_num = 0
                    while get_gpu_num < job.ReqGpuNum:
                        for cur_job in tmp_running_job_list:
                            Msg = f'stop'
                            cur_job.ServerConn.send(Msg.encode())
                            cur_job.realloacted_flag = False
                            cur_job.ready_replay = False
                            cur_job.is_running = False
                            self.is_running_list.remove(cur_job)
                            self.preemt_job_list.append(cur_job)
                            for node in job.node_list:
                                node.job_run_list.remove(cur_job)
                            for gpu in cur_job.gpu_ids:
                                gpu.update_gpu_info(cur_job, 1)
                                allocate_flag_2 = gpu.update_gpu_info(job, 0)
                                if allocate_flag_2 == False:
                                    continue
                                job.gpu_ids.append(gpu)
                                get_gpu_num += 1
                            cur_job.gpu_ids.clear()
                    if get_gpu_num == job.ReqGpuNum:
                        job.is_running = True
                        job.realloacted_flag = True
                        job.ready_replay = True
                        self.is_running_list.append(job)
                        self.highpriority_queue.append(job)
                        logging.info(f'2 Job {job.clientID} is running on node {node.id}')
                        self.waiting_job_list.remove(job)
                        node.job_run_list.append(job)
                        job.node_list.append(node)
                elif job.is_running == False and job.waiting_round < 2:
                    job.waiting_round += 1
                #检查被抢占的作业是否可以重新分配资源
                if self.has_free_resource() and len(self.preemt_job_list) > 0 and len(self.waiting_job_list) == 0:
                    tmp_preemt_job_list = self.preemt_job_list.copy()
                    tmp_preemt_job_list.sort(key=lambda x: x.arrival_time)
                    for preemt_job in tmp_preemt_job_list:
                        for node in self.node_list:
                            if node.free_gpu_num(4) >= preemt_job.ReqGpuNum:
                                tmp_job_gpu_num = preemt_job.ReqGpuNum
                                tmp_gpu_list = node.gpu_list.copy()
                                for gpu in tmp_gpu_list:
                                    if tmp_job_gpu_num == 0:
                                        break
                                    
                                    pre_allocate_flag = gpu.update_gpu_info(preemt_job, 0)
                                    if pre_allocate_flag == False:
                                        continue
                                    preemt_job.gpu_ids.append(gpu)
                                    tmp_job_gpu_num -= 1
                                preemt_job.is_running = True
                                preemt_job.realloacted_flag = True
                                preemt_job.ready_replay = True
                                logging.info(f'3 Job {preemt_job.clientID} is running on node {node.id}')
                                self.preemt_job_list.remove(preemt_job)
                                self.is_running_list.append(preemt_job)
                                node.job_run_list.append(preemt_job)
                                preemt_job.node_list.append(node)
                                break
                
            # time.sleep(self.scheduler_time_interval)
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
        result_log_dir = os.path.join(parent_dir, "result_log")
        
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