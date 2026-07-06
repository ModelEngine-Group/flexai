import socket
import threading
import json
import redis
from ctypes import Structure, c_char, c_int, c_size_t, sizeof
from cffi import FFI
import struct
import logging


logging.basicConfig(level=logging.INFO, format='%(asctime)s [%(levelname)s] %(message)s', datefmt='%Y-%m-%d %H:%M:%S')

def create_redis_connection(redis_config):
    
    r = redis.Redis(
        host=redis_config['RedisConfig']['host'],
        port=redis_config['RedisConfig']['port'],
        db=redis_config['RedisConfig']['db'],
        password=redis_config['RedisConfig']['password']
    )
    return r

def sort_gpu_info(gpu_Info_properties,conf):
    if conf == 1:
        gpu_Info_properties.sort(key=lambda x: json.loads(x[b'gpu_info'])['memory_free'], reverse=True)
        return gpu_Info_properties
    elif conf == 2:
        gpu_Info_properties.sort(key=lambda x: (json.loads(x[b'gpu_info'])['memory_free'], -json.loads(x[b'gpu_info'])['Job_num']), reverse=True)
        return gpu_Info_properties
    elif conf == 3:
        gpu_Info_properties.sort(key=lambda x: json.loads(x[b'gpu_info'])['utilization'], reverse=False)
        return gpu_Info_properties
    else:
        gpu_Info_properties.sort(key=lambda x: (json.loads(x[b'gpu_info'])['memory_free'] / json.loads(x[b'gpu_info'])['memory_total'] * 0.5 + 
                                                (100 - json.loads(x[b'gpu_info'])['utilization']) / 100 * 0.5), reverse=True)
        return gpu_Info_properties


def find_available_gpus(gpuInfoProperties, cnt):
    with open('config.json', 'r') as f:
        config = json.load(f)
    r = create_redis_connection(config)
    avail_gpus = list()
    avail_gpus_properties = list()
    if config['DispatcherMethod']['method'] == 1:
        avail_gpus = list()
        avail_gpus_properties = list()
        for gpuInfoPro in gpuInfoProperties:
            gpu = json.loads(gpuInfoPro[b'gpu_info'])
            gpu['memory_free'] -= 3000 * 2**20
            avail = {
                'gpu_id': gpu['gpu_id'],
                'IP_addr': gpu['IP_addr'],
                'Port': gpu['Port'],
                'HandlerIp': gpu['HandlerIp'],
                'HandlerPort': gpu['HandlerPort']
            }
            avail_gpus.append(avail)
            avail_gpus_properties.append(gpuInfoPro[b'gpu_properties'])
            if len(avail_gpus) == cnt:
                break
        gpuInfoProperties = sort_gpu_info(gpuInfoProperties,1)
        logging.info(f"RoundRobin found GPUs: {avail_gpus}")
        logging.debug(f"RoundRobin found GPU properties: {avail_gpus_properties}")
        return avail_gpus, avail_gpus_properties
    
    elif config['DispatcherMethod']['method'] == 2:
        for gpuInfoPro in gpuInfoProperties:
            gpu = json.loads(gpuInfoPro[b'gpu_info'])
            gpu['memory_free'] -= 5000 *  1024 * 1024
            avail = {
                'gpu_id': gpu['gpu_id'],
                'IP_addr': gpu['IP_addr'],
                'Port': gpu['Port'],
                'HandlerIp': gpu['HandlerIp'],
                'HandlerPort': gpu['HandlerPort']
            }
            avail_gpus.append(avail)
            avail_gpus_properties.append(gpuInfoPro[b'gpu_properties'])
            redis_key = f"{gpu['IP_addr']}:{gpu['gpu_id']}"
            r.hset(redis_key, 'gpu_info', json.dumps(gpu))
            if len(avail_gpus) == cnt:
                break
        logging.info(f"Free Memory GPUs: {avail_gpus}")
        logging.debug(f"Free Memory GPU properties: {avail_gpus_properties}")
        return avail_gpus, avail_gpus_properties
    
    elif config['DispatcherMethod']['method'] == 3:
        for gpuInfoPro in gpuInfoProperties:
            gpu = json.loads(gpuInfoPro[b'gpu_info'])
            gpu['utilization'] += 10
            avail = {
                'gpu_id': gpu['gpu_id'],
                'IP_addr': gpu['IP_addr'],
                'Port': gpu['Port'],
                'HandlerIp': gpu['HandlerIp'],
                'HandlerPort': gpu['HandlerPort']
            }
            avail_gpus.append(avail)
            avail_gpus_properties.append(gpuInfoPro[b'gpu_properties'])
            redis_key = f"{gpu['IP_addr']}:{gpu['gpu_id']}"
            r.hset(redis_key, 'gpu_info', json.dumps(gpu))
            if len(avail_gpus) == cnt:
                break
        logging.info(f"Utilization GPUs: {avail_gpus}")
        logging.debug(f"Utilization GPU properties: {avail_gpus_properties}")
        return avail_gpus, avail_gpus_properties
    
    else:
        for gpuInfoPro in gpuInfoProperties:
            gpu = json.loads(gpuInfoPro[b'gpu_info'])
            gpu['memory_free'] -= 3000 *  1024 * 1024
            gpu['utilization'] += 10
            avail = {
                'gpu_id': gpu['gpu_id'],
                'IP_addr': gpu['IP_addr'],
                'Port': gpu['Port'],
                'HandlerIp': gpu['HandlerIp'],
                'HandlerPort': gpu['HandlerPort']
            }
            avail_gpus.append(avail)
            avail_gpus_properties.append(gpuInfoPro[b'gpu_properties'])
            redis_key = f"{gpu['IP_addr']}:{gpu['gpu_id']}"
            r.hset(redis_key, 'gpu_info', json.dumps(gpu))
            if len(avail_gpus) == cnt:
                break
        logging.info(f"Free Memory and Utilization GPUs: {avail_gpus}")
        logging.debug(f"Free Memory and Utilization GPU properties: {avail_gpus_properties}")
        return avail_gpus, avail_gpus_properties


def handle_message(conn, addr, message, gpuInfoProperties):
    if message.startswith("TypeA:"):
        with open('config.json', 'r') as f:
            config = json.load(f)
        if config['DispatcherMethod']['method'] != 1:
            gpuInfoProperties =  handle_redis()
        logging.info(f"Received message from {addr}: {message}")
        data = message.split(":", 1)[1].split(',') # 获取 "clientID,gpuCount"
        clientID = int(data[0].strip())
        gpuCount = int(data[1].strip().replace('\x00', ''))
        logging.info(f"Client#{clientID}({addr}) requests {gpuCount} GPU(s)")
        allocate_gpus,allocate_gpus_properties = find_available_gpus(gpuInfoProperties, gpuCount)
        for i in range(gpuCount):
            gpuInfo, gpuProp = allocate_gpus[i], allocate_gpus_properties[i]

            gpuInfo_bytes = struct.pack('iH40sH40s', gpuInfo['gpu_id'], gpuInfo['Port'], gpuInfo['IP_addr'].encode('utf-8'), gpuInfo['HandlerPort'], gpuInfo['HandlerIp'].encode('utf-8') )
            conn.sendall(gpuInfo_bytes)

            data_len = len(gpuProp)
            len_bytes = socket.htonl(data_len).to_bytes(8,'big')
            conn.sendall(len_bytes)
            conn.sendall(gpuProp)

            logging.info(f"Sent GPU {gpuInfo['gpu_id']} to client {clientID}")
    else:
        logging.info(f"Received unknown message: {message}")

def handle_client(conn, addr, gpuInfo):  
    logging.info(f"Accepted connection from {addr}")
    logging.debug(f"handle_client: {gpuInfo}")
    while True:
        data = conn.recv(1024).decode()
        if not data:
            break
        handle_message(conn, addr, data, gpuInfo)
    logging.info(f"Connection from {addr} closed")
    conn.close()

def handle_redis():
    gpu_info_properties = list()
    # gpu_properties = list()
    with open('config.json', 'r') as f:
        config = json.load(f)
    conf = config['DispatcherMethod']['method']
    if conf == 1:#简单轮询调度
        logging.info("RoundRobin scheduling")
    elif conf == 2:#资源调度
        logging.info("Free Memory scheduling")
    elif conf == 3:
        logging.info("Utilization scheduling")
    else:
        logging.info("Free Memory and Utilization scheduling")
    r = create_redis_connection(config)
    serv_ip = config['ServerConfig']['serverIp_']
    cursor = 0
    while True:
        cursor, keys = r.scan(cursor=cursor,match='*')
        for redis_key in keys:
            if r.hgetall(redis_key):
                gpu_info_properties.append(r.hgetall(redis_key))
        if cursor == 0:
            break
    gpu_info_properties = sort_gpu_info(gpu_info_properties, conf)
    return gpu_info_properties

def main():
    with open('config.json', 'r') as f:
        config = json.load(f)
    HOST = config['DispatcherConfig']['dpcIp_']
    PORT = config['DispatcherConfig']['dpcPort_']

    # 创建一个TCP socket
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.bind((HOST, PORT))
    server_socket.listen()

    logging.info(f"Server listening on {HOST}:{PORT}")
    
    gpuInfo = handle_redis()

    # 接受并处理客户端连接
    try:
        while True:
            conn, addr = server_socket.accept()
            thread = threading.Thread(target=handle_client, args=(conn, addr, gpuInfo))
            thread.start()
                    

    except KeyboardInterrupt:
        logging.info("Server stopping...")
        server_socket.close()

if __name__ == '__main__':
    main()