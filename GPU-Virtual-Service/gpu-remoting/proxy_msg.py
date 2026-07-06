import socket
import threading
import json
import logging
from ctypes import Structure, c_char, c_int, c_size_t, sizeof
import binascii
import numpy as np
import lz4.frame
import struct
import time
from scheduler.msg_queue import *



logging.basicConfig(level=logging.DEBUG, format='%(asctime)s [%(levelname)s] %(message)s', datefmt='%Y-%m-%d %H:%M:%S')

clientId2gpuInfos = {}
clientId2gpuInfos_lock = threading.Lock()
client_msg_queue = {}
client_msg_queue_lock = threading.Lock()
client_threads = {}

def get_size_t_len():
    return sizeof(c_size_t)


def prepare_gpu_list(clientID, reqestGPUNum, model, batch_size):
    reqMsg = f"TypeA:{clientID},{reqestGPUNum},{model},{batch_size}"
    gpuBasicInfoList = []
    gpuPropList = []

    client = MSG_Client(clientID)
    with client_msg_queue_lock:
        client_msg_queue[clientID] = client
    thread = threading.Thread(target=client.listen, daemon=True).start()
    client_threads[clientID] = thread
    client.send_message(reqMsg)
    received_count = 0
    #TODO: 优化超时机制
    logging.info(f"client{clientID} allocated: {client.allocated}")
    while client.allocated == False:
        time.sleep(1)
    
    while received_count < reqestGPUNum:
        format_str = 'iH40sH40s'
        expected_size = struct.calcsize(format_str)
        # message = client.pubsub.get_message(timeout=1.0)
        message = client.pubsub_data.get_message(timeout=1.0)
        if message['type'] != 'message':
            message = client.pubsub_data.get_message(timeout=1.0)
        data = message['data']
        if data == b"None":
            logging.info(f"Client {client.client_id} received None response")
            return None, None
    # 解析完整消息
        if len(data) < expected_size + 8:  # 基本信息 + 长度字段
            logging.error(f"Received data too short: {len(data)} bytes")
            return None, None
    # 解析 GPU 基本信息
        gpu_id, port, ip_addr_bytes, data_port, data_ip_addr_bytes = struct.unpack(format_str, data[:expected_size])
        ip_addr = ip_addr_bytes.decode('utf-8').rstrip('\x00')
        data_ip_addr = data_ip_addr_bytes.decode('utf-8').rstrip('\x00')

        logging.info(f"GPU ID: {gpu_id}")
        logging.info(f"IP Address: {ip_addr}")
        logging.info(f"Port: {port}")
        logging.info(f"Data IP Address: {data_ip_addr}")
        logging.info(f"Data Port: {data_port}")
        logging.info("---")

        gpuBasicInfoList.append({
            'gpu_id': gpu_id,
            'Port': port,
            'IP_addr': ip_addr,
            'HandlerPort': data_port,
            'HandlerIp': data_ip_addr
        })

        # 解析 GPU 属性长度和数据
        gpu_prop_length_bytes = data[expected_size:expected_size+8]
        gpu_prop_length = socket.ntohl(int.from_bytes(gpu_prop_length_bytes, 'big'))
        gpu_prop_data = data[expected_size+8:expected_size+8+gpu_prop_length]

        if len(gpu_prop_data) < gpu_prop_length:
            logging.error(f"Received GPU properties (len:{len(gpu_prop_data)}) < expected: {gpu_prop_length}")
            return None, None

        decompressed_gpu_prop = lz4.frame.decompress(gpu_prop_data)
        gpuPropList.append(decompressed_gpu_prop)
        received_count += 1
    
    if received_count < reqestGPUNum:
        logging.error(f"Received GPU count ({received_count}) < requested: {reqestGPUNum}")
        return None, None
    
    client.allocated = False
    return gpuBasicInfoList, gpuPropList


def realloc_gpu_list(clientID, reqestGPUNum):
    reqMsg = f"TypeD:{clientID},{reqestGPUNum}"
    gpuBasicInfoList = []
    gpuPropList = []

    if clientID not in client_msg_queue:
        logging.error(f"Client {clientID} not registered yet")
        return None, None
    
    client = client_msg_queue[clientID]
    client.send_message(reqMsg)
    received_count = 0
    deadline = time.time() + 10
    #TODO: 优化超时机制
    logging.info(f"client{clientID} reallocated: {client.allocated}")
    while client.allocated == False:
        print("waiting for allocated")
        time.sleep(1)
    
    while received_count < reqestGPUNum:
        format_str = 'iH40sH40s'
        expected_size = struct.calcsize(format_str)
        # message = client.pubsub.get_message(timeout=1.0)
        message = client.pubsub_data.get_message(timeout=1.0)
        if message['type'] != 'message':
            message = client.pubsub_data.get_message(timeout=1.0)
        data = message['data']
        if data == b"None":
            logging.info(f"Client {client.client_id} received None response")
            return None, None
    # 解析完整消息
        if len(data) < expected_size + 8:  # 基本信息 + 长度字段
            logging.error(f"Received data too short: {len(data)} bytes")
            return None, None
    # 解析 GPU 基本信息
        gpu_id, port, ip_addr_bytes, data_port, data_ip_addr_bytes = struct.unpack(format_str, data[:expected_size])
        ip_addr = ip_addr_bytes.decode('utf-8').rstrip('\x00')
        data_ip_addr = data_ip_addr_bytes.decode('utf-8').rstrip('\x00')

        logging.info(f"GPU ID: {gpu_id}")
        logging.info(f"IP Address: {ip_addr}")
        logging.info(f"Port: {port}")
        logging.info(f"Data IP Address: {data_ip_addr}")
        logging.info(f"Data Port: {data_port}")
        logging.info("---")

        gpuBasicInfoList.append({
            'gpu_id': gpu_id,
            'Port': port,
            'IP_addr': ip_addr,
            'HandlerPort': data_port,
            'HandlerIp': data_ip_addr
        })

        # 解析 GPU 属性长度和数据
        gpu_prop_length_bytes = data[expected_size:expected_size+8]
        gpu_prop_length = socket.ntohl(int.from_bytes(gpu_prop_length_bytes, 'big'))
        gpu_prop_data = data[expected_size+8:expected_size+8+gpu_prop_length]

        if len(gpu_prop_data) < gpu_prop_length:
            logging.error(f"Received GPU properties (len:{len(gpu_prop_data)}) < expected: {gpu_prop_length}")
            return None, None

        decompressed_gpu_prop = lz4.frame.decompress(gpu_prop_data)
        gpuPropList.append(decompressed_gpu_prop)
        received_count += 1
    
    if received_count < reqestGPUNum:
        logging.error(f"Received GPU count ({received_count}) < requested: {reqestGPUNum}")
        return None, None
    client.allocated = False
    return gpuBasicInfoList, gpuPropList


def handle_client_worker(conn, addr):
    logging.info(f"Accepted connection from {addr}")
    clientID = None
    try:
        while True:
            data = conn.recv(get_size_t_len())
            if not data:
                # logging.info("msg is None")
                break

            msg_len = struct.unpack('Q', data)[0]
            if msg_len == 0:
                logging.info("msg_len is 0")
                break
            
            msg = conn.recv(int(msg_len)).decode('utf-8')
            if msg.startswith("GPUQuery:"):
                data = msg.split(":", 1)[1].split(',') 
                clientID = int(data[0].strip())
                gpuCount = int(data[1].strip().replace('\x00', ''))
                model = data[2].strip().replace('\x00', '')
                batch_size = int(data[3].strip().replace('\x00', ''))
                logging.info(f"Client#{clientID}({addr}) requests {gpuCount} GPU(s)")

                with clientId2gpuInfos_lock:
                    isClientIDExist = clientID in clientId2gpuInfos

                gpuBasicInfoList, gpuPropList = None, None

                if not isClientIDExist:
                    
                    gpuBasicInfoList, gpuPropList = prepare_gpu_list(clientID, gpuCount, model, batch_size)
                    if gpuBasicInfoList is None or gpuPropList is None:
                        logging.error(f"Failed to get GPU resources for Client#{clientID}")
                        break
  
                    with clientId2gpuInfos_lock:
                        clientId2gpuInfos[clientID] = {'usedNum': 1, 'gpuInfoList': gpuBasicInfoList, 'gpuPropList': gpuPropList, 'uniqueID': None, 'uniqueIDage': 0}
                        logging.debug(f"Client#{clientID} acquired GPU resources for the first time")
                else:
                    with clientId2gpuInfos_lock:
                        gpuBasicInfoList, gpuPropList = clientId2gpuInfos[clientID]['gpuInfoList'], clientId2gpuInfos[clientID]['gpuPropList']
                        clientId2gpuInfos[clientID]['usedNum'] += 1
                        logging.debug(f"Client#{clientID} has already acquired GPU resources")

                for dev in range(gpuCount):
                    gpuInfo, gpuProp = gpuBasicInfoList[dev], gpuPropList[dev]

                    gpuInfo_byte = struct.pack('iH40sH40s', gpuInfo["gpu_id"], gpuInfo['Port'], gpuInfo['IP_addr'].encode('utf-8'), gpuInfo['HandlerPort'], gpuInfo['HandlerIp'].encode('utf-8'))
                    conn.sendall(gpuInfo_byte)
                    conn.sendall(gpuProp)
                    logging.debug(f"Sent GPU #{dev} info to Client#{clientID}{addr}")
                    
                logging.debug(f"Sent GPU info list to Client#{clientID}{addr} with {len(gpuBasicInfoList)} GPUs")
            elif msg.startswith("GPURealloc:"):
                data = msg.split(":", 1)[1].split(',') 
                clientID = int(data[0].strip())
                gpuCount = int(data[1].strip().replace('\x00', ''))
                logging.info(f"Client#{clientID}({addr}) requests {gpuCount} GPU(s)")

                with clientId2gpuInfos_lock:
                    isClientIDExist = clientID in clientId2gpuInfos

                gpuBasicInfoList, gpuPropList = None, None

                if isClientIDExist:

                    gpuBasicInfoList, gpuPropList = realloc_gpu_list(clientID, gpuCount)
                    if gpuBasicInfoList is None or gpuPropList is None:
                        logging.error(f"Failed to get GPU resources for Client#{clientID}")
                        
                    with clientId2gpuInfos_lock:
                        usedNumtmp = clientId2gpuInfos[clientID]['usedNum']
                        clientId2gpuInfos[clientID] = {'usedNum': usedNumtmp, 'gpuInfoList': gpuBasicInfoList, 'gpuPropList': gpuPropList, 'uniqueID': None, 'uniqueIDage': 0}
                        logging.debug(f"Client#{clientID} reallocated GPU resources")
                else:
                    logging.error(f"Client#{clientID} has not acquired GPU resources")
                    break

                for dev in range(gpuCount):
                    gpuInfo, gpuProp = gpuBasicInfoList[dev], gpuPropList[dev]
                    gpuInfo_byte = struct.pack('iH40sH40s', gpuInfo["gpu_id"], gpuInfo['Port'], gpuInfo['IP_addr'].encode('utf-8'), gpuInfo['HandlerPort'], gpuInfo['HandlerIp'].encode('utf-8'))
                    conn.sendall(gpuInfo_byte)
                    conn.sendall(gpuProp)
                    logging.debug(f"Sent GPU #{dev} info to Client#{clientID}{addr}")

                logging.debug(f"Re Sent GPU info list to Client#{clientID}{addr} with {len(gpuBasicInfoList)} GPUs")
            elif msg.startswith("CommUpdate:"):
                data = msg.split(":", 1)[1].split(',', 1)
                clientID = int(data[0].strip())
                uniqueIDlength = int(data[1].strip())
                
                uniqueID = conn.recv(uniqueIDlength)
                # uniqueID = data[1].strip()[:128]

                with clientId2gpuInfos_lock:
                    if clientID not in clientId2gpuInfos:
                        logging.error(f"Client#{clientID} has not acquired GPU resources")
                        break
                    clientId2gpuInfos[clientID]['uniqueID'] = uniqueID
                    clientId2gpuInfos[clientID]['uniqueIDage'] += 1
                    logging.info(f"Client#{clientID}{addr} updates NCCL comm uniqueID(age: #{clientId2gpuInfos[clientID]['uniqueIDage']})")
            elif msg.startswith("CommQuery:"):
                data = msg.split(":", 1)[1].split(',', 1)
                clientID = int(data[0].strip())
                queryCnt = int(data[1].strip())
                logging.info(f"Client#{clientID}{addr} requests NCCL comm uniqueID(time: #{queryCnt})")

                with clientId2gpuInfos_lock:
                    if clientID in clientId2gpuInfos and 'uniqueID' in clientId2gpuInfos[clientID]:
                        if clientId2gpuInfos[clientID]['uniqueIDage'] == queryCnt:
                            conn.sendall(b'\x01')
                            uniqueID = clientId2gpuInfos[clientID]['uniqueID']
                            conn.sendall(uniqueID)
                            logging.debug(f"Sent uniqueID to Client#{clientID}{addr}")
                        else:
                            conn.sendall(b'\x00')
                            logging.debug(f"Client#{clientID} uniqueID outdated")
                    else:
                        conn.sendall(b'\x00')
                        logging.error(f"Client#{clientID} uniqueID not found")
            else:
                logging.error(f"Received unknown message: {msg}")
    except Exception as e:
        logging.error(f"Error: {e}")
    finally:
        logging.info(f"Connection from Client#{clientID}{addr} closed")
        conn.close()
        if clientID is None:
            return
        with clientId2gpuInfos_lock:
            if clientID not in clientId2gpuInfos:
                return
            clientId2gpuInfos[clientID]['usedNum'] -= 1
            if clientId2gpuInfos[clientID]['usedNum'] == 0:
                del clientId2gpuInfos[clientID]
                logging.info(f"Client#{clientID} released all GPU resources")
                with client_msg_queue_lock:
                    if clientID in client_msg_queue:
                        tmp_client = client_msg_queue[clientID]
                        tmp_client.send_message("STOP")
                        tmp_client.stop()
                        del client_msg_queue[clientID]
                        logging.info(f"Client#{clientID} released all message resources")
                    if clientID in client_threads:
                        del client_threads[clientID]
                        logging.info(f"Client#{clientID} released all thread resources")


def main():

    with open('config.json', 'r') as f:
        config = json.load(f)

    listenIp = config['ClientConfig']['proxyIp_']
    listenPort = config['ClientConfig']['proxyPort_']
    


    listen_proxy = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listen_proxy.bind((listenIp, listenPort))
    listen_proxy.listen()
    logging.info(f"Listening on {listenIp}:{listenPort}")

    try:
        while True:
            conn, addr = listen_proxy.accept()
            logging.debug(f"Accepted connection from {addr}")
            threading.Thread(target=handle_client_worker, args=(conn, addr)).start()
    except KeyboardInterrupt:
        logging.info("Shutting down...")
        listen_proxy.close()


if __name__ == "__main__":
    main()
   