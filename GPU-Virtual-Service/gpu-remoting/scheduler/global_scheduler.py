import os
import sys
import torch
import torch.nn as nn
from torchvision import models, transforms
from collections import deque
import time
import threading
import logging
# from typing import Dict, List, Any
from typing import Dict, List, Any, Optional 

# 将 scheduler 目录添加到 sys.path 中
current_dir = os.path.dirname(os.path.abspath(__file__))
parent_dir = os.path.dirname(current_dir)
sys.path.append(parent_dir)
import socket
import random

import uuid

# from scheduler.requese_handler_5 import *
from scheduler.util import *
from scheduler.gpu_info import*
from scheduler.job import *
from scheduler.node import *
#Version ： 


# 设置日志
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

class Domain:
    def __init__(self, domain_id, ip, port, enable_batching, conn):
        self.domain_id = domain_id
        self.ip = ip
        self.port = port
        self.enable_batching = enable_batching
        self.conn = conn
        self.user_num = 0
        self.job_num = 0
        self.model_user_num: Dict[str, int]= {}

    def get_load(self, arch):
        return self.model_user_num[arch] if arch in self.model_user_num else 0


# Global Scheduler
class GlobalScheduler:
    def __init__(self, domains: list, slo_latency: float = 0.5):
        self.domains = {domain.domain_id: domain for domain in domains}
        self.slo_latency = slo_latency
        self.model_domain_map: Dict[str, List[str]] = {}
        self.stop_event = threading.Event()
        self.enable_batching = True
        

    def get_idle_domain(self) -> Optional[str]:
        if not self.domains:
            return None  # 如果没有可用 Domain，返回 None

        # 找到 user_num + job_num 最小的 Domain
        min_load = float('inf')
        idle_domain_id = None

        for domain_id, domain in self.domains.items():
            total_load = domain.user_num + domain.job_num
            if total_load < min_load:
                min_load = total_load
                idle_domain_id = domain_id

        return idle_domain_id

    def start_user_requests(self, data):
        message = data.split(":", 1)[1].split(',')
        user_id = message[0]
        arch = message[1]
        rps = float(message[2])
        uniform = message[3].lower() == 'true'
        num_requests = int(message[4])
        batch_size = int(message[5])
        msg = f"usereq:{user_id},{arch},{rps},{uniform},{num_requests},{batch_size}"
        if self.enable_batching:
            if arch in self.model_domain_map:
                logger.info(f"Batching Starting user {user_id} requests for Arch {arch} in existing domain")
                best_domain_id = None
                min_latency = float('inf')
                for domain_id in self.model_domain_map[arch]:
                    domain = self.domains[domain_id]
                    # current_latency = gateway.get_load(arch)
                    load_num = domain.get_load(arch)
                    if load_num <= 3:
                        best_domain_id = domain_id
                        logger.info(f"Best domain found: {best_domain_id} with load {load_num}")
                        break
                # if min_latency <= self.slo_latency:
                if best_domain_id is not None and self.domains[best_domain_id].user_num < 3:
                    logger.info(f"Starting user {user_id} requests in domain {best_domain_id} for Arch {arch} with P90 latency {min_latency:.4f}")
                    domain_conn = self.domains[best_domain_id].conn
                    self.domains[best_domain_id].user_num += 1
                    self.domains[best_domain_id].model_user_num[arch] = self.domains[best_domain_id].get_load(arch) + 1
                    domain_conn.sendall(msg.encode())
                else:
                    new_domain_id = self.get_idle_domain()
                    if new_domain_id:
                        if arch not in self.model_domain_map:
                            self.model_domain_map[arch] = []
                        self.model_domain_map[arch].append(new_domain_id)
                        logger.info(f"Scaling Arch {arch} to new domain {new_domain_id} for user {user_id}")
                        domain_conn = self.domains[new_domain_id].conn
                        self.domains[new_domain_id].job_num += 1
                        self.domains[new_domain_id].model_user_num[arch] = self.domains[new_domain_id].get_load(arch) + 1
                        domain_conn.sendall(msg.encode())
                        
                    else:
                        logger.warning(f"No idle domain available, using domain {best_domain_id} with P90 latency {min_latency:.4f}")
                        domain_conn = self.domains[best_domain_id].conn
                        self.domains[best_domain_id].model_user_num[arch] = self.domains[best_domain_id].get_load(arch) + 1
                        # self.domains[best_domain_id].user_num += 1  
                        domain_conn.sendall(msg.encode())
            else:
                domain_id = self.get_idle_domain()
                if domain_id:
                    self.model_domain_map[arch] = [domain_id]
                    logger.info(f"Starting Arch {arch} in new domain {domain_id} for user {user_id}")
                    domain_conn = self.domains[domain_id].conn
                    self.domains[domain_id].job_num += 1
                    self.domains[domain_id].model_user_num[arch] = self.domains[domain_id].get_load(arch) + 1
                    # self.domains[domain_id].user_num += 1
                    domain_conn.sendall(msg.encode())
                else:
                    domain_id = random.choice(list(self.domains.keys()))
                    self.model_domain_map[arch] = [domain_id]
                    logger.warning(f"No idle domain, assigning Arch {arch} to Domain {domain_id} for user {user_id}")
                    domain_conn = self.domains[domain_id].conn
                    self.domains[domain_id].job_num += 1
                    self.domains[domain_id].model_user_num[arch] = self.domains[domain_id].get_load(arch) + 1
                    # self.domains[domain_id].user_num += 1
                    domain_conn.sendall(msg.encode())
        else:
            domain_id = random.choice(list(self.domains.keys()))
            logger.info(f"Non-batching mode: Starting user {user_id} requests in domain {domain_id} for Arch {arch}")
            domain_conn = self.domains[domain_id].conn
            self.domains[domain_id].job_num += 1
            self.domains[domain_id].user_num += 1
            domain_conn.sendall(msg.encode())
    
    def start_Training_requests(self, data):
        message = data.split(":", 1)[1].split(',')
        user_id = message[0]
        arch = message[1]
        batch_size = int(message[2])
        epochs = int(message[3])
        domain = self.get_idle_domain()
        if domain:
            logger.info(f"Starting user {user_id} training requests in domain {domain} for Arch {arch}")
            domain_conn = self.domains[domain].conn
            self.domains[domain].job_num += 1
            self.domains[domain].user_num += 1
            msg = f"useTrain:{user_id},{arch},{batch_size},{epochs}"
            domain_conn.sendall(msg.encode())
        else:
            logger.warning(f"No idle domain available for user {user_id} training requests for Arch {arch}")
    

    def stop_user_requests(self, user_id: str, arch: str):
        if self.domains[list(self.domains.keys())[0]].enable_batching:
            if arch in self.model_domain_map:
                for domain_id in self.model_domain_map[arch]:
                    self.domains[domain_id].stop_user_requests(user_id)
        else:
            for domain_id in self.domains:
                self.domains[domain_id].stop_user_requests(user_id)
    
    def register_domain(self, data, conn):
        # message = data.split(":")
        message = data.split(":", 1)[1].split(',')
        domain_id = message[0]
        ip = message[1]
        port = int(message[2])
        enable_batching = message[3].lower() == 'true'
        # Gateway(domain_id, ip, port, enable_batching)
        logger.info(f"Domain {domain_id} registered with IP {ip} and port {port}, enable_batching: {enable_batching}")
        self.domains[domain_id] = Domain(domain_id, ip, port, enable_batching, conn)
        conn.sendall(f"note:Domain {domain_id} registered successfully.".encode())
        
        
    
    # 处理全局调度器消息
    def handle_glb_message(self, conn, addr):
        while True:
            data = conn.recv(1024)
            if not data:
                break
            message = data.decode()
            if message.startswith('Domain_Regist'):            
                self.register_domain(message, conn)
            elif message.startswith('User_Request_Inference'):
                self.start_user_requests(message)
            elif message.startswith('User_Reqeust_Training'):
                self.start_Training_requests(message)
            elif message.startswith('Domainstop'):
                message = message.split(":")
                domain_id = message[1]
                logger.info(f"Domain {domain_id} stopped.")
                if domain_id in self.domains:
                    del self.domains[domain_id]
                logger.info(f"Domain {domain_id} removed from scheduler.")
            
# 模拟用户请求
def simulate_user_requests(scheduler: GlobalScheduler):
    user_requests = [
        # ("image_data_1", "user1", "resnet18", 5.0, True, 10, 4),  # batch_size = 4
        # ("image_data_2", "user2", "resnet18", 3.0, False, 15, 8),  # batch_size = 8
        # ("image_data_3", "user3", "vgg16", 4.0, True, 8, 6),
        # ("image_data_4", "user4", "resnet18", 6.0, False, 12, 2),
        # ("image_data_5", "user5", "densenet121", 2.0, True, 10, 5),
        # ("image_data_6", "user6", "resnet18", 7.0, False, 15, 3),
        ("image_data_1", "user1", "resnet18", 50, True, 100, 4),
        # ("image_data_2", "user2", "resnet18", 50, True, 100, 8),
        ("image_data_3", "user3", "vgg16", 50, True, 100, 2),
        # ("image_data_4", "user4", "resnet18", 50, True, 100, 4),
        ("image_data_5", "user5", "densenet121", 50, True, 100, 1),
        # ("image_data_6", "user6", "resnet18", 50, True, 50, 2),
        # ("image_data_7", "user7", "resnet18", 40, True, 100, 4),
        # ("image_data_8", "user8", "vgg16", 100, True, 100, 4),
        # ("image_data_9", "user9", "resnet18", 50, True, 100, 8),
        # ("image_data_10", "user10", "densenet121", 6.0, False, 100, 2),
        # ("image_data_11", "user11", "resnet50", 50, True, 100, 4),
        # ("image_data_12", "user12", "resnet50", 30, True, 120, 4),
        # ("image_data_13", "user13", "resnet50", 100, True, 100, 4),
        # ("image_data_14", "user14", "resnet50", 50, False, 100, 4),
        # ("image_data_15", "user15", "resnet50", 10, True, 100, 4),
        # ("image_data_16", "user16", "resnet50", 20, False, 100, 8),
        # ("image_data_17", "user17", "resnet50", 100, True, 100, 4),
        # ("image_data_18", "user18", "resnet50", 10, False, 100, 5),
    ]

    threads = []
    for raw_input, user_id, arch, rps, uniform, num_requests, batch_size in user_requests:
        t = threading.Thread(target=lambda: scheduler.start_user_requests(user_id, arch, rps, uniform, num_requests, batch_size))
        threads.append(t)
        t.start()
        time.sleep(0.05)

    for t in threads:
        t.join()
    
    model_keys = []
    
    for raw_input, user_id, arch, rps, uniform, num_requests, batch_size in user_requests:
        model_key = f"{arch}_{user_id}"
        model_keys.append(model_key)
    return model_keys
    
    
            


def main():
    config = get_config_file()
    glb_Ip = config["GlobalConfig"]["glbIp_"]
    glb_Port = config["GlobalConfig"]["glbPort_"]
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.bind((glb_Ip, glb_Port))
    s.listen()
    logging.info("global Scheduler started listen at %s:%d", glb_Ip, glb_Port)
    thread_List = []
    
    glb_scheduler = GlobalScheduler([], slo_latency=0.5)
    
    try:
        while True:
            conn, addr = s.accept()
            logging.info("Connected by %s", addr)
            thread = threading.Thread(target=glb_scheduler.handle_glb_message, args=(conn, addr))
            thread.start()
            thread_List.append(thread)
    except KeyboardInterrupt:
        logging.info("Server is shutting down...")
        s.close()
        for thread in thread_List:
            thread.join()
        logging.info("All threads have been terminated.")
        glb_scheduler.stop_event.set()
        #TODO: 关闭所有域
        # for domain in glb_scheduler.domains.values():
        #     domain.stop()
        logging.info("All domains have been stopped.")

# 主函数
if __name__ == "__main__":
    main()
    # # 测试批处理模式
    # domains_batching = [
    #     Gateway(domain_id="domain1", ip="10.26.42.231", port=42231, enable_batching=True),
    #     Gateway(domain_id="domain2", ip="10.26.42.232", port=42232, enable_batching=True),
    #     Gateway(domain_id="domain3", ip="10.26.42.226", port=22, enable_batching=True),
    # ]
    # scheduler_batching = GlobalScheduler(domains_batching, slo_latency=0.5)
    # logger.info("Starting Global Scheduler with batching...")
    # simulate_user_requests(scheduler_batching)
    # time.sleep(15)
    # for domain in domains_batching:
    #     for arch in ["resnet18", "vgg16", "densenet121"]:
    #         throughput = domain.get_throughput(arch)
    #         if throughput > 0:
    #             logger.info(f"+++Domain {domain.domain_id} - Arch {arch} Avg Throughput: {throughput:.2f} samples/sec")

    # for domain in domains_batching:
    #     domain.stop()
    # logger.info("Batching simulation completed.")

    # 测试非批处理模式
    # domains_no_batching = [
    #     Gateway(domain_id="domain1", ip="10.26.42.231", port=42231, enable_batching=False),
    #     Gateway(domain_id="domain2", ip="10.26.42.232", port=42232, enable_batching=False),
    #     Gateway(domain_id="domain3", ip="10.26.42.226", port=22, enable_batching=False),
    # ]
    # scheduler_no_batching = GlobalScheduler(domains_no_batching, slo_latency=0.5)
    # logger.info("Starting Global Scheduler without batching...")
    # model_keys =  simulate_user_requests(scheduler_no_batching)
    # time.sleep(15)
    # # for domain in domains_no_batching:
    # #     for arch in ["resnet18", "vgg16", "densenet121"]:
    # #         throughput = domain.get_throughput(arch)
    # #         if throughput > 0:
    # #             logger.info(f"+++Domain {domain.domain_id} - Arch {arch} Avg Throughput: {throughput:.2f} samples/sec")
    # for domain in domains_no_batching:    
    #     for model_key in model_keys:
    #         throughput = domain.get_throughput(model_key)    
    #         if throughput > 0:
    #             logger.info(f"+++ Domain {domain.domain_id} arch {model_key}, Avg Throughput: {throughput:.2f} samples/sec")


    # for domain in domains_no_batching:
    #     domain.stop()
    # logger.info("Non-batching simulation completed.")