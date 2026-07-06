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

import sys
import os

# 将 scheduler 目录添加到 sys.path 中
current_dir = os.path.dirname(os.path.abspath(__file__))
parent_dir = os.path.dirname(current_dir)
sys.path.append(parent_dir)

from scheduler.cluster import *

import threading
import time
import random

# r = redis_connection()

logging.basicConfig(level=logging.INFO, format='%(asctime)s [%(levelname)s] %(message)s', datefmt='%Y-%m-%d %H:%M:%S')
# 公共注册频道
REGISTER_CHANNEL = "channel:register"

# 模拟客户端类
class MSG_Client:
    def __init__(self, client_id):
        self.client_id = client_id
        self.redis_conn = redis_connection()
        # self.client_channel = client_id  # 接收服务器消息
        self.status_channel = f"{self.client_id}_status"  # 状态消息频道
        self.data_channel = self.client_id  # 数据消息频道
        self.server_channel = f"server_{client_id}"  # 发送给服务器
        self.pubsub_status = self.redis_conn.pubsub()  # 状态订阅
        self.pubsub_data = self.redis_conn.pubsub()    # 数据订阅
        self.pubsub_status.subscribe(self.status_channel)
        self.pubsub_data.subscribe(self.data_channel)
        # self.pubsub = self.redis_conn.pubsub()
        # self.pubsub.subscribe(self.client_channel)
        self.running = True
        # 客户端启动时注册自己
        self.registered = False  # 注册状态
        self.allocated = False
        self.register()

    def register(self):
        # 发送注册消息到公共频道
        self.redis_conn.publish(REGISTER_CHANNEL, self.client_id)
        print(f"Client {self.client_id} registered itself")

    def listen(self):
        print(f"Client {self.client_id} started, listening on {self.status_channel}")
        while self.running:
            message = self.pubsub_status.get_message(timeout=1.0)
            if message and message['type'] == 'message':
                data = message['data'].decode('utf-8')
                if data == "REGISTERED":
                    self.registered = True
                elif data == "ALLOCATED":
                    self.allocated = True
                print(f"Client {self.client_id} received: {message['data'].decode()}")
            time.sleep(0.01)

    def send_message(self, message):
        timeout = 5  # 最多等待 5 秒
        start_time = time.time()
        # while not self.registered and time.time() - start_time < timeout:
        while not self.registered:
            time.sleep(1)
        if not self.registered:
            logging.error(f"Client {self.client_id} failed to register within {timeout} seconds")
            return
        full_message = f"{message}"
        self.redis_conn.publish(self.server_channel, full_message)
        print(f"Client {self.client_id} sent: {full_message}")

    def stop(self):
        self.running = False
        # self.pubsub.unsubscribe(self.client_channel)
        self.pubsub_status.unsubscribe(self.status_channel)
        self.pubsub_data.unsubscribe(self.data_channel)
        # self.redis_conn.close()
        print(f"Client {self.client_id} stopped")

# 服务器类
class MSG_Server:
    def __init__(self):
        self.clients = {}
        self.redis_conn = redis_connection()
        self.pubsub = self.redis_conn.pubsub()
        self.running = True
        # 订阅注册频道
        self.pubsub.subscribe(REGISTER_CHANNEL)
    
    def register_client(self, client_id):
        if client_id not in self.clients:
            self.clients[client_id] = {
                "status_channel": f"{client_id}_status",
                "data_channel": client_id,
                "send_channel": f"server_{client_id}"
            }
            self.pubsub.subscribe(self.clients[client_id]["send_channel"])
            logging.info(f"Server registered client {client_id}")
            self.redis_conn.publish(self.clients[client_id]["status_channel"], "REGISTERED")

    def send_message(self, target_client_id, message):
        if target_client_id in self.clients:
            channel = self.clients[target_client_id]["receive_channel"]
            full_message = f"server:{message}"
            self.redis_conn.publish(channel, full_message)
            print(f"Server sent to {target_client_id}: {full_message}")

    def listen(self):
        print(f"Server started, listening for registrations and messages")
        while self.running:
            message = self.pubsub.get_message(timeout=1.0)
            if message and message['type'] == 'message':
                channel = message['channel'].decode()
                data = message['data'].decode()
                if channel == REGISTER_CHANNEL:
                    # 处理注册消息
                    self.register_client(data)
                else:
                    # 处理客户端消息
                    client_id = channel.split("_")[-1]
                    print(f"Server received from {client_id}: {data}")
            time.sleep(0.01)

    def send_to_client(self, gpu_info_list, clientID):
        # print(f'send to clientID:{clientID}')
        logging.info(f'Send to clientID:{clientID}')
        client_channel = clientID  # 使用 clientID 作为频道
        
        # self.msg_server.redis_conn.publish(self.msg_server.clients[client_id]["status_channel"], "ALLOCATED")
        self.redis_conn.publish(self.clients[str(client_channel)]["status_channel"], "ALLOCATED")

        if str(clientID) not in self.clients:
            # print(f"Client {clientID} not registered yet")
            logging.error(f"Client {clientID} not registered yet")
            return
        print(f'gpu_info_list: {gpu_info_list}')
        for gpu_info in gpu_info_list:
            if gpu_info is None:
                self.redis_conn.publish(self.clients[clientID]["data_channel"], None)
                # print(f"Sent None to {clientID}")
                logging.info(f"Sent None to {clientID}")
                return
            avail_gpu = {
                'gpu_id': gpu_info.gpu_id,
                'IP_addr': gpu_info.IP_addr,
                'Port': gpu_info.Port,
                'HandlerIp': gpu_info.HandlerIp,
                'HandlerPort': gpu_info.HandlerPort
            }
            gpu_properties = gpu_info.gpu_properties

            gpuInfo_bytes = struct.pack(
                'iH40sH40s',
                avail_gpu['gpu_id'],
                avail_gpu['Port'],
                avail_gpu['IP_addr'].encode('utf-8'),
                avail_gpu['HandlerPort'],
                avail_gpu['HandlerIp'].encode('utf-8')
            )
            data_len = len(gpu_properties)
            len_bytes = socket.htonl(data_len).to_bytes(8, 'big')
            full_message = gpuInfo_bytes + len_bytes + gpu_properties


            self.redis_conn.publish(client_channel, full_message)
            # print(f'data: {full_message}')
            # print(f"Sent GPU info to {clientID}: {len(full_message)} bytes")
            logging.info(f'Sent GPU info to {clientID}: {len(full_message)} bytes')

        

    def run(self):#dummy test
        while self.running:
            if self.clients:
                target_id = random.choice(list(self.clients.keys()))
                self.send_message(target_id, f"Hello at {time.ctime()}")
            time.sleep(5)

    def stop_client(self, client_id):
        if client_id in self.clients:
            self.pubsub.unsubscribe(self.clients[client_id]["send_channel"])
            del self.clients[client_id]
            logging.info(f"Sent stop message to client {client_id}")
        else:
            logging.error(f"Client {client_id} not found")
    
    def stop(self):
        self.running = False
        self.pubsub.unsubscribe(REGISTER_CHANNEL)
        for client_id in self.clients:
            self.pubsub.unsubscribe(self.clients[client_id]["send_channel"])
        # self.redis_conn.close()
        print("Server stopped")
