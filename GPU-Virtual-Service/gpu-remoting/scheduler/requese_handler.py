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
import numpy as np
import uuid
import socket
import signal
import subprocess
from concurrent.futures import ThreadPoolExecutor
#Version ：
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


# 设置日志
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)
# 输入预处理
preprocess = transforms.Compose([
    transforms.Resize((224, 224)),
    transforms.ToTensor(),
    transforms.Normalize(mean=[0.485, 0.456, 0.406], std=[0.229, 0.224, 0.225])
])

# 为张量添加时间戳和批次大小的类
class TimestampedTensor:
    def __init__(self, tensor: torch.Tensor, batch_size: int):
        self.tensor = tensor
        self.timestamp = time.time()
        self.batch_size = batch_size

class ModelManager:
    def __init__(self, num_gpus: int = 2):
        self.models: Dict[str, nn.Module] = {}
        self.gpu_assignments: Dict[str, int] = {}  # 模型 key 到 GPU ID 的映射
        self.lock = threading.Lock()
        self.num_gpus = num_gpus  # 可用 GPU 数量
        logger.info(f"ModelManager initialized with {self.num_gpus} GPUs")

    def get_or_create_model(self, key: str, enable_batching) -> nn.Module:
        with self.lock:
            if key in self.models:
                logger.info(f"Reusing existing model: {key} on GPU {self.gpu_assignments[key]}")
                return self.models[key]
            try:
                if not enable_batching:
                    arch = key.split('_')[0]  # 假设 key 是 "arch_userid" 格式
                else:
                    arch = key
                # 根据 key 分配 GPU（简单哈希方式）
                gpu_id = hash(key) % self.num_gpus if self.num_gpus > 0 else 0
                device = torch.device(f"cuda:{gpu_id}" if torch.cuda.is_available() and self.num_gpus > 0 else "cpu")
                
                model_fn = getattr(models, arch, None)
                if model_fn is None or not callable(model_fn):
                    raise ValueError(f"Invalid architecture: {arch}")
                logger.info(f"Choosing device: {device} for model {key}")
                model = model_fn(pretrained=True).to(device)
                model.eval()
                self.models[key] = model
                self.gpu_assignments[key] = gpu_id  # 记录分配的 GPU ID
                logger.info(f"Created new model: {key} on GPU {gpu_id}")
                return model
            except Exception as e:
                logger.error(f"Failed to load model {key}: {e}")
                raise

    def remove_model(self, key: str):
        with self.lock:
            if key in self.models:
                del self.models[key]
                del self.gpu_assignments[key]
                logger.info(f"Removed model: {key}")
                
# Gateway 类
class Gateway:
    def __init__(self, domain_id, ip, port, glbconn, timeout: float = 0.2, max_latency: float = 2.0, enable_batching: bool = True, max_concurrent_requests: int = 10):
        self.domain_id = domain_id
        self.region_ip = ip
        self.region_port = port
        self.glbconn = glbconn
        # self.model_manager = ModelManager()
        self.num_gpus = torch.cuda.device_count() if torch.cuda.is_available() else 0
        self.model_manager = ModelManager(num_gpus=self.num_gpus)
        self.buffers: Dict[str, deque] = {}
        self.request_maps: Dict[str, Dict[str, Tuple[str, int]]] = {}
        self.timeout = timeout
        self.max_latency = max_latency
        self.locks: Dict[str, threading.Lock] = {}
        self.results: Dict[str, torch.Tensor] = {}
        self.running: Dict[str, bool] = {}
        self.threads: Dict[str, threading.Thread] = {}
        self.latency: Dict[str, float] = {}
        self.latency_history: Dict[str, deque] = {}
        self.first_inference: Dict[str, bool] = {}
        self.last_request_time: Dict[str, float] = {}
        self.enable_batching = enable_batching #是否开启Batching
        self.max_concurrent_requests = max_concurrent_requests
        self.active_requests: Dict[str, int] = {}  # 每个用户的活跃请求数
        self.request_semaphore: Dict[str, threading.Semaphore] = {}  # 每个用户的并发控制
        self.user_models: Dict[str, str] = {}  # 用户到模型 key 的映射
        self.model_keys = []
        # 吞吐量统计
        self.throughput_history: Dict[str, deque] = {}
        
        # 多用户请求生成相关
        self.user_threads: Dict[str, threading.Thread] = {}
        self.user_configs: Dict[str, dict] = {}
        self.user_sleep_times: Dict[str, list] = {}
        self.user_lock = threading.Lock()
        self.user_running: Dict[str, bool] = {}
        self.thread_pool = ThreadPoolExecutor(max_workers=20)
        
        # 非批处理模式下的请求线程
        self.request_threads: Dict[str, threading.Thread] = {}
        
        # GPU 资源信息
        self.stop_flag = False
        config = get_config_file()
        local_ip = config['RequestConfig']['localIp_']
        r = redis_connection()
        self.gpu_list = []
        for i in range(2):
            redis_key = f"{local_ip}:{i}"
            gpu_info_pro = r.hgetall(redis_key)
            gpu = GPU_info(gpu_info_pro)
            self.gpu_list.append(gpu)
            logger.info(f"GPU {i} info: {gpu}")
        
        #处理Training请求
        self.train_model_gpus: Dict[str, int] = {}  # 模型到 GPU 的映射
        
        if self.enable_batching:
            self.idle_check_thread = threading.Thread(target=self._check_idle_models, daemon=True)
            self.idle_check_thread.start()

    def _generate_sleep_times(self, rps: float, uniform: bool, num_requests: int) -> list:
        if rps > 0:
            if uniform:
                return [1/rps] * num_requests
            else:
                return np.random.exponential(scale=1/rps, size=num_requests).tolist()
        return [0] * num_requests

    def _get_model_resources(self, arch: str) -> tuple:
        if arch not in self.buffers:
            self.buffers[arch] = deque()
            self.request_maps[arch] = {}
            self.locks[arch] = threading.Lock()
            self.running[arch] = True
            self.latency[arch] = 0.0
            self.latency_history[arch] = deque(maxlen=50)
            self.first_inference[arch] = True
            self.last_request_time[arch] = time.time()
            self.active_requests[arch] = 0
            self.request_semaphore[arch] = threading.Semaphore(self.max_concurrent_requests)
            self.threads[arch] = threading.Thread(target=self._check_and_process_batch, args=(arch,), daemon=True)
            self.threads[arch].start()
        return self.buffers[arch], self.request_maps[arch], self.locks[arch]

    def preprocess_input(self, raw_input: Any, batch_size: int,) -> TimestampedTensor:
        try:
            input_tensor = torch.randn(batch_size, 3, 224, 224)
            device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
            return TimestampedTensor(input_tensor.to(device), batch_size)
        except Exception as e:
            logger.error(f"Preprocess error: {e}")
            raise

    def process_batch(self, model: nn.Module, batch: torch.Tensor, arch: str) -> tuple:
        try:
            with torch.no_grad():
                # if torch.cuda.is_available():
                #     batch = batch.cuda()
                # start_time = time.time()
                # outputs = model(batch)
                # latency = time.time() - start_time
                # throughput = batch.size(0) / latency
                
                if torch.cuda.is_available():
                    gpu_id = self.model_manager.gpu_assignments[arch]  # 获取模型的 GPU ID
                    batch = batch.to(f"cuda:{gpu_id}")
                start_time = time.time()
                outputs = model(batch)
                latency = time.time() - start_time
                throughput = batch.size(0) / latency
                
                if arch in self.throughput_history:
                    self.throughput_history[arch].append(throughput)
                    avg_throughput = np.mean(list(self.throughput_history[arch])) 
                else:
                    # self.throughput_history[arch] = deque(maxlen=50)
                    self.throughput_history[arch] = deque()
                    
                    avg_throughput = throughput
                
                # with self.locks[arch]:
                if self.first_inference[arch]:
                    logger.info(f"Cold start for Arch {arch}, latency {latency:.4f} not counted")
                    self.first_inference[arch] = False
                else:
                    self.latency_history[arch].append(latency)
                    logger.info(f"_+_+_+!!Arch {arch} latency history: {self.latency_history[arch]}")
                    latencies = list(self.latency_history[arch])
                    logger.info(f"_+_+_+!!Arch {arch} latencies: {latencies}")
                    if latencies:
                        logger.info(f"_+_+_+!!Arch {arch} latencies: {latencies}")
                        self.latency[arch] = np.percentile(latencies, 90)
                        logger.info(f"_+_+_+!!Arch {arch} latency: {self.latency[arch]}")
                    else:
                        self.latency[arch] = 0.0
                    
                
                logger.info(f"Arch {arch}, Batch_size {batch.size(0)}, Throughput: {throughput:.2f} samples/sec, Latency: {latency:.4f} seconds, avg Throughput: {avg_throughput}, P90 Latency: {self.latency[arch]:.4f}")
            return outputs, latency
        except Exception as e:
            logger.error(f"Batch processing error: {e}")
            raise

    def process_single_request(self, raw_input: Any, user_id: str, arch: str, request_id: str, batch_size: int):
        """非批处理模式下处理用户请求，使用用户绑定的模型"""
        model_key = f"{arch}_{user_id}"
        self.latency_history[model_key] = deque(maxlen=50)
        if model_key not in self.throughput_history:
            # self.throughput_history[model_key] = deque(maxlen=50)
            self.throughput_history[model_key] = deque()
            self.model_keys.append(model_key)

        if user_id not in self.active_requests:
            self.active_requests[user_id] = 0
        if user_id not in self.request_semaphore:
            self.request_semaphore[user_id] = threading.Semaphore(self.max_concurrent_requests)

        with self.request_semaphore[user_id]:
            with self.user_lock:
                self.active_requests[user_id] += 1
                if user_id not in self.user_models:
                    self.user_models[user_id] = model_key
            logger.info(f"_+_+_+Processing single request {request_id} for User {user_id} on Arch {arch}, Batch Size: {batch_size}")
            model = self.model_manager.get_or_create_model(model_key, False)
            input_tensor = self.preprocess_input(raw_input, batch_size)
            try:
                # with torch.no_grad():
                #     if torch.cuda.is_available():
                #         input_tensor.tensor = input_tensor.tensor.cuda()
                #     start_time = time.time()
                #     output = model(input_tensor.tensor)
                #     latency = time.time() - start_time
                #     throughput = batch_size / latency
                with torch.no_grad():
                    if torch.cuda.is_available():
                        gpu_id = self.model_manager.gpu_assignments[model_key]  # 获取模型的 GPU ID
                        # input_tensor.tensor = input_tensor.tensor.to(f"cuda:{gpu_id}")
                        batch = input_tensor.tensor.to(f"cuda:{gpu_id}")
                    start_time = time.time()
                    output = model(batch)
                    latency = time.time() - start_time
                    throughput = batch_size / latency
                    
                    with self.user_lock:
                        if self.first_inference.get(model_key, True):
                            avg_throughput = throughput
                            self.first_inference[model_key] = False
                        else:
                            self.latency_history[model_key].append(latency)
                            latencies = list(self.latency_history[model_key])
                            if latencies:
                                self.latency[model_key] = np.percentile(latencies, 90)
                            else:
                                self.latency[model_key] = 0.0
                            self.throughput_history[model_key].append(throughput)
                            avg_throughput = np.mean(list(self.throughput_history[model_key]))
                        self.results[request_id] = output
                        self.active_requests[user_id] -= 1
                    
                    logger.info(f"Processed single request for User {user_id} on Arch {arch}, Batch Size: {batch_size}, Latency: {latency:.4f} seconds, Throughput: {throughput:.2f} samples/sec, Avg Throughput: {avg_throughput:.2f} samples/sec")
            except Exception as e:
                logger.error(f"Single request processing failed: {e}")
                with self.user_lock:
                    self.active_requests[user_id] -= 1

    def send_result(self, arch: str, request_id: str, result: torch.Tensor):
        user_id, _ = self.request_maps[arch].get(request_id, (None, None))
        self.results[request_id] = result
        logger.info(f"Result sent to User {user_id} for Request {request_id} (Arch: {arch}): {result.shape}")

    def handle_request(self, raw_input: Any, user_id: str, arch: str, batch_size: int) -> str:
        request_id = str(uuid.uuid4())
        if self.enable_batching:
            buffer, request_map, lock = self._get_model_resources(arch)
            with lock:
                input_tensor = self.preprocess_input(raw_input, batch_size)
                buffer.append(input_tensor)
                request_map[request_id] = (user_id, batch_size)
                self.last_request_time[arch] = time.time()
                logger.info(f"Received request {request_id} from User {user_id} for Arch {arch}, Batch Size: {batch_size}, Buffer size: {len(buffer)}")
        else:
            # thread = threading.Thread(target=self.process_single_request, args=(raw_input, user_id, arch, request_id, batch_size), daemon=True)
            # self.request_threads[request_id] = thread
            # thread.start()    
            self.thread_pool.submit(self.process_single_request, raw_input, user_id, arch, request_id, batch_size)
            logger.info(f"Received and processing request {request_id} from User {user_id} for Arch {arch}, Batch Size: {batch_size} (no batching)")
        return request_id

    def _check_and_process_batch(self, arch: str):
        buffer, request_map, lock = self._get_model_resources(arch)
        
        while self.running.get(arch, False):
            with lock:
                if buffer:
                    total_batch_size = sum(item.batch_size for item in buffer)
                    if total_batch_size > 0 and (len(buffer) >= 1 and (time.time() - buffer[0].timestamp > self.timeout or total_batch_size >= max(item.batch_size for item in buffer))):
                        batch_list = list(buffer)
                        batch = torch.cat([item.tensor for item in batch_list], dim=0)
                        logger.info(f"Processing batch of size {batch.size(0)} for Arch {arch}")
                        model = self.model_manager.get_or_create_model(arch, True)
                        try:
                            results, latency = self.process_batch(model, batch, arch)
                            offset = 0
                            request_ids = list(request_map.keys())[:len(batch_list)]
                            for i, req_id in enumerate(request_ids):
                                batch_size = batch_list[i].batch_size
                                self.send_result(arch, req_id, results[offset:offset + batch_size])
                                offset += batch_size
                                del request_map[req_id]
                            buffer.clear()
                        except Exception as e:
                            logger.error(f"Batch processing failed for {arch}: {e}")
            time.sleep(0.01)

    def _check_idle_models(self):
        while True:
            with self.user_lock:
                current_time = time.time()
                for arch in list(self.running.keys()):
                    last_time = self.last_request_time.get(arch, current_time)
                    if current_time - last_time > 10 and self.running.get(arch, False):
                        logger.info(f"Model {arch} in Domain {self.domain_id} idle for 10s, shutting down")
                        self.running[arch] = False
                        if arch in self.threads and self.threads[arch].is_alive():
                            self.threads[arch].join()
                        if arch in self.buffers:
                            del self.buffers[arch]
                        if arch in self.request_maps:
                            del self.request_maps[arch]
                        if arch in self.locks:
                            del self.locks[arch]
                        # if arch in self.latency:
                        #     del self.latency[arch]
                        # if arch in self.latency_history:
                        #     del self.latency_history[arch]
                        if arch in self.threads:
                            del self.threads[arch]
            time.sleep(1)

    def _generate_requests(self, user_id: str, arch: str, batch_size: int):
        config = self.user_configs[user_id]
        sleep_times = self.user_sleep_times[user_id]
        for i in range(config['num_requests']):
            if not self.user_running.get(user_id, False):
                break
            raw_input = None
            self.handle_request(raw_input, user_id, arch, batch_size)
            sleep_time = sleep_times[i]
            logger.info(f"User {user_id} - Generated request {i+1}/{config['num_requests']}, Batch Size: {batch_size}, sleeping for {sleep_time:.4f}s")
            time.sleep(sleep_time)

    def start_user_requests(self, user_id: str, arch: str, rps: float, uniform: bool, num_requests: int, batch_size: int):
        with self.user_lock:
            if user_id in self.user_threads and self.user_threads[user_id].is_alive():
                logger.info(f"User {user_id} is already running")
                return
            
            self.user_configs[user_id] = {
                'rps': rps,
                'uniform': uniform,
                'num_requests': num_requests,
                'batch_size': batch_size
            }
            self.user_sleep_times[user_id] = self._generate_sleep_times(rps, uniform, num_requests)
            self.user_running[user_id] = True
            
            thread = threading.Thread(
                target=self._generate_requests, args=(user_id, arch, batch_size), daemon=True
            )
            self.user_threads[user_id] = thread
            thread.start()
            logger.info(f"Started request generator for User {user_id} on Arch {arch} with {'uniform' if uniform else 'poisson'} distribution, Batch Size: {batch_size}")

    def stop_user_requests(self, user_id: str):
        with self.user_lock:
            if user_id in self.user_running:
                self.user_running[user_id] = False
                if user_id in self.user_threads and self.user_threads[user_id].is_alive():
                    self.user_threads[user_id].join()
                if user_id in self.user_models:
                    self.model_manager.remove_model(self.user_models[user_id])
                    del self.user_models[user_id]
                if user_id in self.active_requests:
                    del self.active_requests[user_id]
                if user_id in self.request_semaphore:
                    del self.request_semaphore[user_id]
                logger.info(f"Stopped request generator for User {user_id}")

    def get_result(self, request_id: str) -> Optional[torch.Tensor]:
        return self.results.get(request_id, None)

    def stop(self):
        for arch in self.running:
            self.running[arch] = False
        for thread in self.threads.values():
            thread.join()
        self.stop_flag = True
        
        with self.user_lock:
            for user_id in self.user_running:
                self.user_running[user_id] = False
                if user_id in self.user_threads and self.user_threads[user_id].is_alive():
                    self.user_threads[user_id].join()
                if user_id in self.user_models:
                    self.model_manager.remove_model(self.user_models[user_id])
                    del self.user_models[user_id]
            for thread in self.request_threads.values():
                if thread.is_alive():
                    thread.join()
        logger.info(f"Domain {self.domain_id} stopped")

    def get_load(self, arch: str) -> float:
        return self.latency.get(arch, 0.0) if self.enable_batching else 0.0

    def get_throughput(self, arch: str) -> float:
        # logger.info(f"domain id {self.domain_id} get throughput {arch}, len {len(self.throughput_history)}")
        # logger.info(f"Throughput history keys: {list(self.throughput_history.keys())}")
        if arch in self.throughput_history and self.throughput_history[arch]:
            return np.mean(list(self.throughput_history[arch]))
        return 0.0
    
    def get_latency(self, arch: str) -> float:
        # logger.info(f"_+_+!!domain id {self.domain_id} get latency {arch},")
        # logger.info(f"_+_+!!domain id {self.domain_id} get latency {arch}, ")
        
        if arch in self.latency:
            return self.latency[arch]
        return 0.0
    
    def handle_client_request(self, message):
        data = message.split(":", 1)[1].split(',')
        user_id = data[0]
        arch = data[1]
        rps = float(data[2])
        # uniform = bool(int(data[3]))
        uniform = data[3].lower() == 'true'
        num_requests = int(data[4])
        batch_size = int(data[5])
        logger.info(f"Received user request: {message}")
        if self.enable_batching:
            logger.info(f"Enable_Batching Starting user requests for User {user_id} on Arch {arch}, RPS: {rps}, Uniform: {uniform}, Num Requests: {num_requests}, Batch Size: {batch_size}")
            self.start_user_requests(user_id, arch, rps, uniform, num_requests, batch_size)
        else:
            logger.info(f"Non-Batching Starting user requests for User {user_id} on Arch {arch}, Batch Size: {batch_size}")
            self.start_user_requests(user_id, arch, rps, uniform, num_requests, batch_size)
    
    def handle_train_request(self, message):
        data = message.split(":", 1)[1].split(',')
        user_id = data[0]
        arch = data[1]
        batch_size = int(data[2])
        num_epochs = int(data[3])
        #python scripts/workloads/imageNetTrain.py -a arch -b batch_size -e num_epochs --first_batch --gpu gpu
        logger.info(f"Received user training request: {message}")
        # 处理用户训练请求
        self.gpu_list.sort(key=lambda x: x.memory_free, reverse=True)
        gpu_id = self.gpu_list[0].gpu_id
        self.gpu_list[0].memory_free -= 3000 * 2**20
        self.train_model_gpus[arch] = gpu_id
        # command = [
        #     "python", "scripts/workloads/imageNetTrain.py",
        #     "-a", arch,
        #     "-b", str(batch_size),
        #     "-e", str(num_epochs),
        #     "--first_batch",
        #     "--gpu", str(gpu_id)
        # ]
        command = f"python scripts/workloads/imageNetTrain_.py -a {arch} -b {batch_size} --epochs {num_epochs} --first_batch --gpu {gpu_id} --batch_rate 0.1"
        logger.info(f"Executing command: {command}")
        subprocess.Popen(command, shell=True)
         
        #异步接受GlbobalServer的消息
    def receive_messages(self,):
        start_time = time.time()
        while True:
            try:
                data = self.glbconn.recv(1024)
                if not data:
                    break
                message = data.decode()
                logger.info(f"Received message from Global Server: {message}")
                if message.startswith("usereq:"):
                    logger.info(f"Received user request: {message}")
                    self.handle_client_request(message)
                elif message.startswith("useTrain:"):
                    logger.info(f"Received user training request: {message}")
                    # 处理用户训练请求
                    self.handle_train_request(message)
                elif message == "stop":
                    self.stop()
                    break
                elif message.startswith("note:"):
                    print(message)
                # 处理消息
            except KeyboardInterrupt as e:
                # logger.error(f"Error receiving message: {e}")
                self.stop()
                break
    

def signal_handler(sig, frame, glb_socket, domain_id, gateway):
    """处理 Ctrl+C 信号，发送 Domainstop 消息并关闭 socket"""
    logger.info("Received Ctrl+C, sending Domainstop...")
    model_total_throughput = {}
    model_total_latency = {}
    try:
        if gateway.enable_batching:
            for arch in list(gateway.running.keys()):
                logger.info(f"=============== {arch} in Domain {domain_id} ===============")
                throughput = gateway.get_throughput(arch)
                latency = gateway.get_latency(arch)
                if latency > 0:
                    logger.info(f"Model< {arch} > in Domain {domain_id} latency: {latency:.4f}")
                else:
                    logger.info(f"Model< {arch} > in Domain {domain_id} latency: 0.0")
                if throughput > 0:
                    logger.info(f"Model< {arch} > in Domain {domain_id} throughput: {throughput:.2f}")
        else:
            for model_key in list(gateway.model_keys):
                logger.info(f"=============== {model_key} in Domain {domain_id} ===============")
                throughput = gateway.get_throughput(model_key)
                latency = gateway.get_latency(model_key)
                if latency > 0:
                    logger.info(f"Model< {model_key} > in Domain {domain_id} latency: {latency:.4f}")
                    model, user = model_key.split('_')
                    if model not in model_total_latency:
                        model_total_latency[model] = []
                    model_total_latency[model].append(latency)
                else:
                    logger.info(f"Model< {model_key} > in Domain {domain_id} latency: 0.0")
                if throughput > 0:
                    logger.info(f"Model< {model_key} > in Domain {domain_id} throughput: {throughput:.2f}")
                    model, user = model_key.split('_')
                    if model not in model_total_throughput:
                        model_total_throughput[model] = []
                    model_total_throughput[model].append(throughput)
        
        if not gateway.enable_batching:
            # 计算平均吞吐量和延迟
            logger.info(f"\n================= no batching {domain_id} Summary =================")
            for model in model_total_latency.keys():  # 遍历所有模型
                # 计算平均 latency
                avg_latency = np.mean(model_total_latency[model])
                # 计算平均 throughput
                avg_throughput = np.mean(model_total_throughput.get(model, []))  # 防止 KeyError
                # 输出模型信息（latency + throughput）
                logger.info(
                    f"Model< {model} > in Domain {domain_id} | "
                    f"Avg Latency: {avg_latency:.4f} | "
                    f"Avg Throughput: {avg_throughput:.2f}"
                )
        stop_msg = f"Domainstop:{domain_id}"
        glb_socket.sendall(stop_msg.encode())
        time.sleep(0.5)  # 确保消息发送完成
    except Exception as e:
        logger.error(f"Failed to send Domainstop: {e}")
    finally:
        glb_socket.close()
        sys.exit(0)  # 退出程序

def main():
    config = get_config_file()
    domain_id = config['RequestConfig']['domain_id_']
    domain_ip = config['RequestConfig']['ReqIp_']
    domain_port = config['RequestConfig']['ReqPort_']
    domian_enable_batching = config['RequestConfig']['enable_batching_']
    
    # gateway = Gateway(domain_id, domain_ip, domain_port, enable_batching=True)
    #注册GateWay
    glb_ip = config['GlobalConfig']['glbIp_']
    glb_port = config['GlobalConfig']['glbPort_']
    glb_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    glb_socket.connect((glb_ip, glb_port))
    register_msg = f"Domain_Regist:{domain_id},{domain_ip},{domain_port},{domian_enable_batching}"
    glb_socket.sendall(register_msg.encode())

      # 设置 Ctrl+C 信号处理
    
    gateway = Gateway(domain_id, domain_ip, domain_port, glb_socket, enable_batching=domian_enable_batching)

    signal.signal(signal.SIGINT, lambda sig, frame: signal_handler(sig, frame, glb_socket, domain_id, gateway))


    receive_thread = threading.Thread(target=gateway.receive_messages, daemon=True)
    receive_thread.start()
    
    while not gateway.stop_flag:
        time.sleep(1)
    
    try:
        while not gateway.stop_flag:
            time.sleep(1)
    except KeyboardInterrupt:
        pass  # 由 signal_handler 处理
    finally:
        glb_socket.close()
        logger.info("Gateway stopped")
    
if __name__ == "__main__":
    main()