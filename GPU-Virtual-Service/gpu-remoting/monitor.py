import socket
import threading
import json
import pynvml
import os 
import numpy as np
import time
import ctypes
from cffi import FFI
import lz4.frame
import struct
import logging

from runtime_config import create_redis_connection, load_runtime_config

# 删除 CUDA_VISIBLE_DEVICES 环境变量
if 'CUDA_VISIBLE_DEVICES' in os.environ:
    del os.environ['CUDA_VISIBLE_DEVICES']

logging.basicConfig(level=logging.INFO, format='%(asctime)s [%(levelname)s] %(message)s', datefmt='%Y-%m-%d %H:%M:%S')


# 加载 CUDA 运行时库
cuda = ctypes.CDLL("/usr/local/cuda/lib64/libcudart.so")
cudaDeviceProp_size = 728

def get_device_properties_bytes(device_id=0):
    # 直接分配一个与 cudaDeviceProp 相同大小的字节数组
    buffer = (ctypes.c_byte * cudaDeviceProp_size)()

    # 调用 cudaGetDeviceProperties 并传入字节数组
    result = cuda.cudaGetDeviceProperties(ctypes.byref(buffer), device_id)
    if result != 0:
        raise RuntimeError(f"CUDA error: {result}")
    
    return buffer


#在初始化的时候调用，初始化redis里的GPU信息
def get_gpu_info():
    # global r
    config = load_runtime_config()
    logging.debug("Get GPU info")
    gpuInfo = list()
    compressed_datas = list()
    # 初始化NVML，这对于使用pynvml库是必需的
    pynvml.nvmlInit()

    # 获取GPU数量
    dev_cnt = pynvml.nvmlDeviceGetCount()
    # torch.cuda.device_count()
    logging.info(f"Total {dev_cnt} GPU(s) found")
    IP_addr = config['ServerConfig']['serverIp_']
    Port = config['ServerConfig']['serverPort_']

    logging.debug(f"IP_addr: {IP_addr}, Port: {Port}")
    r = create_redis_connection()

    # 打印每个GPU的内存信息和利用率
    for i in range(dev_cnt):
        gpu_info = {}
        # 获取当前GPU的句柄
        handle = pynvml.nvmlDeviceGetHandleByIndex(i)
        gpu_info['gpu_id'] = i
        gpu_info['IP_addr'] = IP_addr
        gpu_info['Port'] = Port
        
        # 获取内存信息
        mem_info = pynvml.nvmlDeviceGetMemoryInfo(handle)
        # free_mem = mem_info.free
        # total_mem = mem_info.total
        # used_mem = total_mem - free_mem
        gpu_info['memory_total'] = mem_info.total
        gpu_info['memory_free'] = mem_info.free
        gpu_info['memory_used'] = mem_info.used
        
        # 获取GPU利用率
        util = pynvml.nvmlDeviceGetUtilizationRates(handle)
        # gpu_util = util.gpu
        gpu_info['utilization'] = util.gpu

        gpu_info['Job_num'] = 0
        gpu_info['GPU_num'] = dev_cnt

        gpu_info['HandlerIp'] = config['DatasetHandlerConfig']['handlerIp_']
        gpu_info['HandlerPort'] = config['DatasetHandlerConfig']['handlerPort_']
        gpuInfo.append(gpu_info)

        # 获取GPU的properties
        gpu_properties = get_device_properties_bytes(i)
        compressed_data = lz4.frame.compress(gpu_properties)
        compressed_datas.append(compressed_data)

        #插入redis, key = '192.168.0.208:0'
        redis_key = f"{IP_addr}:{i}"
        # r.set(redis_key, json.dumps(gpu_info))#key = '192.168.0.208:0'
        r.hset(redis_key, mapping={'gpu_info': json.dumps(gpu_info), 'gpu_properties': compressed_data})

    logging.debug(gpuInfo)

    # 清理，释放NVML资源
    pynvml.nvmlShutdown()
        
#更新redis中GPU信息
def update_gpu_info():
    # global r
    config = load_runtime_config()
    logging.info("Updating GPU info...")
    gpuInfo = list()
    # 初始化NVML，这对于使用pynvml库是必需的
    pynvml.nvmlInit()
    # 获取GPU数量
    dev_cnt = pynvml.nvmlDeviceGetCount()
    # torch.cuda.device_count()
    logging.info(f"Total {dev_cnt} GPU(s) found")
    # IP_addr = '192.168.0.208'
    Ip_addr = config['ServerConfig']['serverIp_']

    r = create_redis_connection()

    
    # 更新每个GPU的内存信息和利用率
    for i in range(dev_cnt):
        redis_key = f"{Ip_addr}:{i}"
        # now_gpuInfo_bytes = r.get(redis_key)
        now_gpuInfo_bytes = r.hget(redis_key, 'gpu_info')
        if now_gpuInfo_bytes:
            now_gpuInfo = json.loads(now_gpuInfo_bytes.decode('utf-8'))
        else:
            now_gpuInfo = {}
        #更新gpu信息
        handle = pynvml.nvmlDeviceGetHandleByIndex(i)
        mem_info = pynvml.nvmlDeviceGetMemoryInfo(handle)
        now_gpuInfo['memory_total'] = mem_info.total
        now_gpuInfo['memory_free'] = mem_info.free
        now_gpuInfo['memory_used'] = mem_info.used
        # 获取GPU利用率
        util = pynvml.nvmlDeviceGetUtilizationRates(handle)
        # gpu_util = util.gpu
        now_gpuInfo['utilization'] = util.gpu
        # now_gpuInfo['Job_num'] = i + 1
        gpuInfo.append(now_gpuInfo)
        # r.set(redis_key, json.dumps(now_gpuInfo))
        r.hset(redis_key, 'gpu_info', json.dumps(now_gpuInfo))
    logging.debug(gpuInfo)
    # 清理，释放NVML资源
    pynvml.nvmlShutdown()

#client任务结束后，减少GPU的Job_num
def reduce_gpu_job(data):
    # 解析data字符串，提取GPU ID
    gpu_ids = data.split(':')[1].split(',')
    config = load_runtime_config()
    r = create_redis_connection()
 
    Ip_addr = config['ServerConfig']['serverIp_']
    for gpu_id in gpu_ids:
        key = f"{Ip_addr}:{gpu_id}"
        if r.exists(key):
            gpu_info = json.loads(r.hget(key, 'gpu_info').decode('utf-8'))
            gpu_info['Job_num'] -= 1
            r.hset(key, 'gpu_info', json.dumps(gpu_info))
            gpu_info_update = json.loads(r.hget(key, 'gpu_info').decode('utf-8'))
            logging.debug("gpu_info:", gpu_info_update)
        else:
            logging.info(f"GPU {gpu_id} not found")


def handle_new_job(data):
        # 解析data字符串，提取GPU ID
    gpu_ids = data.split(':')[1].split(',')
    config = load_runtime_config()
    r = create_redis_connection()
    Ip_addr = config['ServerConfig']['serverIp_']
    for gpu_id in gpu_ids:
        key = f"{Ip_addr}:{gpu_id}"
        if r.exists(key):
            # gpu_info = json.loads(r.get(key).decode('utf-8'))
            gpu_info = json.loads(r.hget(key, 'gpu_info').decode('utf-8'))
            gpu_info['Job_num'] += 1
            r.hset(key, 'gpu_info', json.dumps(gpu_info))
            # gpu_info_update = json.loads(r.get(key).decode('utf-8'))
            gpu_info_update = json.loads(r.hget(key, 'gpu_info').decode('utf-8'))
            logging.debug("gpu_info:", gpu_info_update)
        else:
            logging.info(f"GPU {gpu_id} not found")

def collect_gpu_utilization():
    config = load_runtime_config()
    # 初始化pynvml
    pynvml.nvmlInit()
    dev_cnt = pynvml.nvmlDeviceGetCount()

    IP_addr = config['ServerConfig']['serverIp_']
    Port = config['ServerConfig']['serverPort_']
    r = create_redis_connection()

    # 创建一个字典来存储每个GPU的utilization数据
    gpu_utilizations = {i: [] for i in range(dev_cnt)}

    while True:
        for i in range(dev_cnt):
            handle = pynvml.nvmlDeviceGetHandleByIndex(i)
            # 获取当前GPU的utilization
            util = pynvml.nvmlDeviceGetUtilizationRates(handle)
            gpu_util = util.gpu

            # 添加到数组中
            gpu_utilizations[i].append(gpu_util)

            # 当数组长度达到200时，计算90分位数，并更新到Redis中，然后清空数组
            if len(gpu_utilizations[i]) == 200:
                redis_key = f"{IP_addr}:{i}"
                # now_gpuInfo_bytes = r.get(redis_key)
                now_gpuInfo_bytes = r.hget(redis_key, 'gpu_info')
                if now_gpuInfo_bytes:
                    now_gpuInfo = json.loads(now_gpuInfo_bytes.decode('utf-8'))
                else:
                    now_gpuInfo = {}
                # 计算90分位数
                percentile_90th = np.percentile(gpu_utilizations[i], 90)
                
                 #更新gpu信息
                handle = pynvml.nvmlDeviceGetHandleByIndex(i)
                mem_info = pynvml.nvmlDeviceGetMemoryInfo(handle)
                now_gpuInfo['memory_total'] = mem_info.total
                now_gpuInfo['memory_free'] = mem_info.free
                now_gpuInfo['memory_used'] = mem_info.used

                now_gpuInfo['utilization'] = percentile_90th

                # r.set(redis_key, json.dumps(now_gpuInfo))
                r.hset(redis_key, 'gpu_info', json.dumps(now_gpuInfo))

                # 清空数组
                gpu_utilizations[i].clear()

        # 每隔3秒收集一次数据
        time.sleep(3)

    # 清理pynvml
    pynvml.nvmlShutdown()


def handle_message(conn, data, addr):
    #新客户端连接后，更新GPU信息
    if data == 'new_client_run':
        logging.info(f"Received message from server: {data}")
        update_gpu_info()
    #客户端跑完任务后，减少GPU的Job_num
    elif data.startswith('client_stop:'):
        logging.info(f"Received message from server: {data}")
        update_gpu_info()
        reduce_gpu_job(data)
        logging.info(f"Finish job handle")
        conn.sendto(b"finish", addr)
        # handle_new_job(conn, addr, data)
    elif data.startswith('client_runjob:'):
        logging.info(f"Received message from server: {data}")
        update_gpu_info()
        handle_new_job(data)     
    else:
        logging.info(f"Unknown message from server")


def handle_server(conn):
    logging.info(f"Accepted connection from server")
    while True:
        data = conn.recv(1024).decode()
        if not data:
            break
        logging.debug(f"Received message from server: {data}")
        handle_message(conn, data)
    logging.info(f"Connection from server closed")
    conn.close()




def handle_server_tcp(conn, addr):  
    logging.info(f"Accepted connection from {addr}")
    while True:
        data = conn.recv(1024).decode()
        if not data:
            break
        handle_message(conn, data, addr)
    logging.info(f"Connection from {addr} closed")
    conn.close()

def schedule_update():
        update_gpu_info()
        threading.Timer(240, schedule_update).start()  # 600秒 = 10分钟

def main():
    #一开始先初始化redis里的服务器信息，然后启动TCP连接，监听Server请求
    get_gpu_info()
    config = load_runtime_config()

    HOST = config['MonitorConfig']['monitorIp_']
    PORT = config['MonitorConfig']['monitorPort_']
    logging.debug(f"HOST: {HOST}, PORT: {PORT}")
    logging.info("Server listening on")

    
    # 启动GPU utilization数据收集线程
    gpu_thread = threading.Thread(target=collect_gpu_utilization)
    gpu_thread.daemon = True  # 设置为守护线程，这样主线程结束时该线程也会结束
    gpu_thread.start()

     # 启动一个单独的线程来运行 schedule_update
    update_thread = threading.Thread(target=schedule_update)
    update_thread.start()

    #改成tcp
    # 接受并处理客户端连接
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.bind((HOST, PORT))
    server_socket.listen()
    try:
        while True:
            conn, addr = server_socket.accept()
            thread = threading.Thread(target=handle_server_tcp, args=(conn, addr,))
            thread.start()
                    

    except KeyboardInterrupt:
        gpu_thread.join()
        update_thread.join()
        server_socket.close()
        logging.info("Monitor stopped")


if __name__ == '__main__':
    main()
