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
import subprocess
from datetime import datetime
import os

logging.basicConfig(level=logging.INFO, format='%(asctime)s [%(levelname)s] %(message)s', datefmt='%Y-%m-%d %H:%M:%S')

def run_command(command_str, log_file_path):
    with open(log_file_path, 'w') as log_file:
        subprocess.run(command_str, shell=True, stdout=log_file, stderr=log_file)


def handle_request(client_id, req_gpu_num, priority, model, type, batchsize, job_type, batch_rate, epoch, url = 0):
    cv_command = [
        "FLEXGV_CLIENT_ID={}".format(client_id),
        "FLEXGV_PRIORITY={}".format(priority),
        "FLEXGV_REQ_NUM={}".format(req_gpu_num),
        "FLEXGV_MODEL={}".format(model),
        "FLEXGV_BATCH_SIZE={}".format(batchsize),
        "LD_PRELOAD=./out/lib64/libcuda_hook.so",
        "LD_LIBRARY_PATH=./out/lib64:$LD_LIBRARY_PATH",
        "python", "scripts/workloads/imageNetTrain.py",
        "-a", model,
        "-b", str(batchsize),
        "--gpu", "0",
        "-j", "2",
        "--epochs", str(epoch)
    ]
    if type == 1:
        cv_command.append("--evaluate")
        cv_command.append("--pretrained")
    elif type == 0:
        if batch_rate > 0:
            cv_command.append("--batch_rate")
            cv_command.append(f"{batch_rate}")
    
    gpt_command = [
        "FLEXGV_CLIENT_ID={}".format(client_id),
        "FLEXGV_PRIORITY={}".format(priority),
        "FLEXGV_REQ_NUM={}".format(req_gpu_num),
        "FLEXGV_MODEL=gpt",
        "FLEXGV_BATCH_SIZE={}".format(batchsize),
        "LD_PRELOAD=./out/lib64/libcuda_hook.so",
        "LD_LIBRARY_PATH=./out/lib64:$LD_LIBRARY_PATH",
        "python", "scripts/workloads/large-language-model/clmTrainWoPrep.py",
        "-b", str(batchsize),
        "-e", str(epoch)
    ]
    
    bert_command = [
        "FLEXGV_CLIENT_ID={}".format(client_id),
        "FLEXGV_PRIORITY={}".format(priority),
        "FLEXGV_REQ_NUM={}".format(req_gpu_num),
        "FLEXGV_MODEL=BERT",
        "FLEXGV_BATCH_SIZE={}".format(batchsize),
        "LD_PRELOAD=./out/lib64/libcuda_hook.so",
        "LD_LIBRARY_PATH=./out/lib64:$LD_LIBRARY_PATH",
        "python", "scripts/workloads/text-classification/glueTrainWoPrep.py",
        "-b", str(batchsize),
        "-e", str(epoch),
    ]
    
    ddp_command = [
        "FLEXGV_CLIENT_ID={}".format(client_id),
        "FLEXGV_PRIORITY={}".format(priority),
        "FLEXGV_REQ_NUM={}".format(req_gpu_num),
        "FLEXGV_MODEL={}".format(model),
        "FLEXGV_BATCH_SIZE={}".format(batchsize),
        "LD_PRELOAD=./out/lib64/libcuda_hook.so",
        "LD_LIBRARY_PATH=./out/lib64:$LD_LIBRARY_PATH",
        "python", "scripts/workloads/imageNetTrainDDP.py",
        "/home/djh/dataset/ImageNet-1K",
        "-a", model,
        "-b", str(batchsize),
        "--epochs", str(epoch),
        "--dist-url", f"tcp://127.0.0.1:166{url}"
    ]
    if type == 1:
        ddp_command.append("--evaluate")
        ddp_command.append("--pretrained")
    elif type == 0:
        if batch_rate > 0:
            ddp_command.append("--batch_rate")
            ddp_command.append(f"{batch_rate}")
    
    if job_type == 0:
        command_str = " ".join(cv_command)
        logging.info(f'Executing command: {command_str}')
    elif job_type == 1:
        command_str = " ".join(gpt_command)
        logging.info(f'Executing command: {command_str}')
    elif job_type == 2:
        command_str = " ".join(bert_command)
        logging.info(f'Executing command: {command_str}')
    elif job_type == 3:
        command_str = " ".join(ddp_command)
        logging.info(f'Executing command: {command_str}')
        
    # 获取今天的日期
    date = datetime.now().strftime('%Y-%m-%d')
    # 创建目录路径
    result_dir = f'result_{date}'
    # 如果目录不存在，则创建目录
    if not os.path.exists(result_dir):
        os.makedirs(result_dir)
    # 设置日志文件路径
    log_file_path = os.path.join(result_dir, f'output{client_id}.log')
    # with open(log_file_path, 'w') as log_file:
    #     # 执行命令并将输出重定向到日志文件
    #     subprocess.run(command_str, shell=True, stdout=log_file, stderr=log_file)
    
    process = Process(target=run_command, args=(command_str, log_file_path))
    process.start()
    return process
    
    
def main():
    # Trace 30
    handle_request(1001, 1, 0, "mobilenet_v2", 0, 32, 0, 0.05, 1)      #  小
    time.sleep(1)
    handle_request(1002, 1, 0, "GPT3", 0, 2, 1, 0.8, 4)             # LLM, 大
    time.sleep(1)
    handle_request(1003, 1, 0, "resnet50", 0, 32, 0, 0.3, 4)          #  中
    time.sleep(1)
    handle_request(1004, 1, 0, "Bert", 0, 64, 2, 0.3, 4)              # NLP, 中
    time.sleep(200)
    handle_request(1005, 1, 0, "shufflenet_v2_x0_5", 0, 32, 0, 0.05, 1)  # CV, 小
    time.sleep(1)
    handle_request(1006, 1, 0, "resnet18", 0, 64, 0, 0.3, 4)          #  中
    time.sleep(1)
    handle_request(1007, 1, 0, "GPT3", 0, 2, 1, 0.8, 2)             # LLM, 大
    time.sleep(1)
    handle_request(1008, 1, 0, "mobilenet_v2", 0, 32, 0, 0.05, 1)     # CV, 小
    time.sleep(200)
    handle_request(1009, 1, 0, "Bert", 0, 64, 2, 0.3, 4)              # NLP, 中
    time.sleep(1)
    handle_request(1010, 1, 0, "vgg16", 0, 64, 0, 0.3, 4)             #  中
    time.sleep(1)
    handle_request(1011, 1, 0, "shufflenet_v2_x0_5", 0, 32, 0, 0.05, 1)  # CV, 小
    time.sleep(1)
    handle_request(1012, 1, 0, "resnet18", 0, 32, 0, 0.05, 1)         # CV, 小
    time.sleep(200)
    handle_request(1013, 1, 0, "GPT3", 0, 2, 1, 0.8, 12)             # LLM, 大
    time.sleep(1)
    handle_request(1014, 1, 0, "mobilenet_v2", 0, 32, 0, 0.05, 1)     # 小
    time.sleep(1)
    handle_request(1015, 1, 0, "Bert", 0, 64, 2, 0.3, 4)              # NLP, 中
    time.sleep(1)
    handle_request(1016, 1, 0, "resnet50", 0, 64, 0, 0.3, 4)          # CV, 中
    time.sleep(200)
    
    handle_request(1017, 1, 0, "shufflenet_v2_x0_5", 0, 32, 0, 0.05, 1)  # CV, 小
    time.sleep(1)
    handle_request(1018, 1, 0, "GPT3", 0, 2, 1, 0.05, 1)             # LLM, 小
    time.sleep(1)
    handle_request(1019, 2, 0, "vgg16", 0, 32, 0, 0.05, 1)            # 小
    time.sleep(1)
    handle_request(1020, 1, 0, "mobilenet_v2", 0, 32, 0, 0.05, 1)     # CV, 小
    time.sleep(200)
    handle_request(1021, 1, 0, "Bert", 0, 64, 2, 0.3, 4)              # NLP, 中
    time.sleep(1)
    handle_request(1022, 1, 0, "resnet50", 0, 32, 0, 0.05, 1)         # CV, 小
    time.sleep(1)
    handle_request(1023, 1, 0, "GPT3", 0, 2, 1, 0.05, 1)             # LLM, 小
    time.sleep(1)
    handle_request(1024, 1, 0, "shufflenet_v2_x0_5", 0, 32, 0, 0.05, 1)  # CV, 小
    time.sleep(200)
    handle_request(1025, 1, 0, "resnet18", 0, 32, 0, 0.05, 1)         # CV, 小
    time.sleep(1)
    handle_request(1026, 1, 0, "mobilenet_v2", 0, 32, 0, 0.05, 1)     # CV, 小
    time.sleep(1)
    handle_request(1027, 1, 0, "Bert", 0, 32, 2, 0.05, 1)             # NLP, 小
    time.sleep(1)
    handle_request(1028, 1, 0, "resnet50", 0, 64, 0, 0.3, 4)          # CV, 中
    time.sleep(200)
    handle_request(1029, 1, 0, "GPT3", 0, 2, 1, 0.05, 1)             # LLM, 小
    time.sleep(1)
    handle_request(1030, 1, 0, "vgg16", 0, 32, 0, 0.05, 1)            # CV, 小
    # 按时间顺序启动任务
    processes = []
    ddp_counter = 0  # 用于生成递增的url
 
    # 可选：等待所有进程完成
    for p in processes:
        p.join()
    logging.info("All processes completed.")

if __name__ == "__main__":
    # 配置日志
    logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
    main()
    
    
    
    