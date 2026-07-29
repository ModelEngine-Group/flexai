import socket
import threading
import json
import logging
from ctypes import Structure, c_char, c_int, c_size_t, sizeof
import binascii
import numpy as np
import lz4.frame
import struct

from runtime_config import load_runtime_config

logging.basicConfig(level=logging.DEBUG, format='%(asctime)s [%(levelname)s] %(message)s', datefmt='%Y-%m-%d %H:%M:%S')

clientId2gpuInfos = {}
clientId2gpuInfos_lock = threading.Lock()

def get_size_t_len():
    return sizeof(c_size_t)

def connect_to_dispatcher(dpc_ip, dpc_port):
    try:
        proxy = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        proxy.connect((dpc_ip, dpc_port))
        return proxy
    except Exception as e:
        logging.error(f"Failed to connect to dispatcher({dpc_ip}:{dpc_port}): {e}")
        return None

def receive_message(conn):
    # 首先接收4字节的长度信息
    length_bytes = conn.recv(8)
    if not length_bytes:
        return None
    
    # 将网络字节序转换回整数
    message_length = socket.ntohl(int.from_bytes(length_bytes, 'big'))
    
    # 接收实际数据
    data = b''
    while len(data) < message_length:
        chunk = conn.recv(min(message_length - len(data), 4096))
        if not chunk:
            return None
        data += chunk
    
    # 解码并解析 JSON 数据为 Python 对象
    return json.loads(data.decode('utf-8'))

def prepare_gpu_list(clientID, reqestGPUNum, priority, conn, conn_lock):
    reqMsg = f"TypeA:{clientID},{reqestGPUNum},{priority}"
    gpuBasicInfoList = []
    gpuPropList = []

    with conn_lock:
        conn.sendall(reqMsg.encode('utf-8'))
        for dev in range(reqestGPUNum):

            format_str = 'iH40sH40s'
            expected_size = struct.calcsize(format_str)
            received_data = bytearray()
            while len(received_data) < expected_size:
                packet = conn.recv(expected_size - len(received_data))
                if not packet:
                    break
                received_data.extend(packet)

            if len(received_data) < expected_size:
                logging.error(f"Received GPUlist data (len:{len(received_data)}) is less than expected size: {expected_size}")
                return None, None

            gpu_id, port, ip_addr_bytes, data_port, data_ip_addr_bytes = struct.unpack(format_str, received_data)
            ip_addr = ip_addr_bytes.decode('utf-8').rstrip('\x00')
            data_ip_addr = data_ip_addr_bytes.decode('utf-8').rstrip('\x00')
            
            logging.info(f"GPU ID: {gpu_id}")
            logging.info(f"IP Address: {ip_addr}")
            logging.info(f"Port: {port}")
            logging.info(f"Data IP Address: {data_ip_addr}")
            logging.info(f"Data Port: {data_port}")
            logging.info("---")
            gpuBasicInfoList.append({'gpu_id': gpu_id, 'Port': port, 'IP_addr': ip_addr, 'HandlerPort': data_port, 'HandlerIp': data_ip_addr})

            gpuProp_length_bytes = conn.recv(get_size_t_len())
            if not gpuProp_length_bytes:
                logging.error("the length of gpu (gpuid={gpu_id}) is 0")
                return None, None
            gpuProp_length_bytes = socket.ntohl(int.from_bytes(gpuProp_length_bytes, 'big'))

            recv_gpuProp = bytearray()
            while len(recv_gpuProp) < gpuProp_length_bytes:
                packet = conn.recv(gpuProp_length_bytes - len(recv_gpuProp))
                if not packet:
                    break
                recv_gpuProp.extend(packet)
            if len(recv_gpuProp) < gpuProp_length_bytes:
                logging.error(f"Received GPU properties (len:{len(recv_gpuProp)}) is less than expected size: {gpuProp_length_bytes}")
                return None, None
            decompressed_gpuProp = lz4.frame.decompress(recv_gpuProp)
            gpuPropList.append(decompressed_gpuProp)
    
    return gpuBasicInfoList, gpuPropList

def handle_client_worker(conn, addr, proxy, proxy_lock): 
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
                priority = int(data[2].strip().replace('\x00', ''))
                logging.info(f"Client#{clientID}({addr}) requests {gpuCount} GPU(s)")

                with clientId2gpuInfos_lock:
                    isClientIDExist = clientID in clientId2gpuInfos

                gpuBasicInfoList, gpuPropList = None, None

                if not isClientIDExist:
                    while True:
                        gpuBasicInfoList, gpuPropList = prepare_gpu_list(clientID, gpuCount, priority, proxy, proxy_lock)
                        if gpuBasicInfoList is None or gpuPropList is None:
                            logging.error(f"Failed to get GPU resources for Client#{clientID}")
                        else:
                            break
  
                    with clientId2gpuInfos_lock:
                        clientId2gpuInfos[clientID] = {'usedNum': 1, 'gpuInfoList': gpuBasicInfoList, 'gpuPropList': gpuPropList, 'uniqueID': None, 'uniqueIDage': 0}
                        logging.debug(f"Client#{clientID} acquired GPU resources for the first time")
                else:
                    with clientId2gpuInfos_lock:
                        gpuBasicInfoList, gpuPropList = clientId2gpuInfos[clientID]['gpuInfoList'], clientId2gpuInfos[clientID]['gpuPropList']
                        clientId2gpuInfos[clientID]['usedNum'] += 1
                        logging.debug(f"Client#{clientID} has already acquired GPU resources")

                # with clientId2gpuInfos_lock:
                #     if clientID not in clientId2gpuInfos:
                #         gpuBasicInfoList, gpuPropList = prepare_gpu_list(clientID, gpuCount, proxy, proxy_lock)
                #         clientId2gpuInfos[clientID] = {'usedNum': 1, 'gpuInfoList': gpuBasicInfoList, 'gpuPropList': gpuPropList, 'uniqueID': None, 'uniqueIDage': 0}
                #         logging.debug(f"Client#{clientID} acquired GPU resources for the first time")
                #     else:
                #         gpuBasicInfoList, gpuPropList = clientId2gpuInfos[clientID]['gpuInfoList'], clientId2gpuInfos[clientID]['gpuPropList']
                #         clientId2gpuInfos[clientID]['usedNum'] += 1
                #         logging.debug(f"Client#{clientID} has already acquired GPU resources")

                for dev in range(gpuCount):
                    gpuInfo, gpuProp = gpuBasicInfoList[dev], gpuPropList[dev]

                    gpuInfo_byte = struct.pack('iH40sH40s', gpuInfo["gpu_id"], gpuInfo['Port'], gpuInfo['IP_addr'].encode('utf-8'), gpuInfo['HandlerPort'], gpuInfo['HandlerIp'].encode('utf-8'))
                    conn.sendall(gpuInfo_byte)
                    conn.sendall(gpuProp)

                    logging.debug(f"Sent GPU #{dev} info to Client#{clientID}{addr}")
                
                logging.debug(f"Sent GPU info list to Client#{clientID}{addr} with {len(gpuBasicInfoList)} GPUs")
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

def main():
    config = load_runtime_config()

    dpcIp = config['DispatcherConfig']['dpcIp_']
    dpcPort = config['DispatcherConfig']['dpcPort_']
    listenIp = config['ClientConfig']['proxyIp_']
    listenPort = config['ClientConfig']['proxyPort_']
    
    proxy = connect_to_dispatcher(dpcIp, dpcPort)
    if proxy is None:
        return

    logging.info(f"Connected to dispatcher({dpcIp}:{dpcPort})")
    proxy_lock = threading.Lock()

    listen_proxy = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listen_proxy.bind((listenIp, listenPort))
    listen_proxy.listen()
    logging.info(f"Listening on {listenIp}:{listenPort}")

    try:
        while True:
            conn, addr = listen_proxy.accept()
            logging.debug(f"Accepted connection from {addr}")
            threading.Thread(target=handle_client_worker, args=(conn, addr, proxy, proxy_lock)).start()
    except KeyboardInterrupt:
        logging.info("Shutting down...")
        listen_proxy.close()
        proxy.close()

    proxy.close()

if __name__ == "__main__":
    main()
   
