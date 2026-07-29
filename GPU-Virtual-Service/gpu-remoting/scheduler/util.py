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
import pandas as pd
from scheduler.gpu_info import *
from scheduler.job import *
import pulp
from runtime_config import create_redis_connection, get_job_info_path, load_runtime_config

logging.basicConfig(level=logging.INFO, format='%(asctime)s [%(levelname)s] %(message)s', datefmt='%Y-%m-%d %H:%M:%S')


def get_config_file():
    return load_runtime_config()

def query_job_info(model, batch_size):
    df = pd.read_csv(get_job_info_path())
    
    # 根据 model 和 batch_size 查询
    result = df[(df['model'] == model) & (df['batchsize'] == batch_size)]
    
    return result


def redis_connection():
    return create_redis_connection()

def redis_job_connection():
    return create_redis_connection(db_key="jobdb")
    
def allocate_gpus(gpu_free_memory, k, m):
    """
    为新任务分配 GPU 资源。
    :param gpu_free_memory: 字典，键为 GPU ID,值为空闲显存
    :param k: 需要分配的 GPU 数量
    :param m: 每个 GPU 所需的最小显存
    :return: 分配的 GPU 列表，或 None(无解)
    """
    # 筛选可用 GPU
    available_gpus = {i: free for i, free in gpu_free_memory.items() if free >= m}
    
    # 检查是否足够
    if len(available_gpus) < k:
        print(f"无解：可用 GPU 数量 ({len(available_gpus)}) < 需求 ({k})")
        return None
    
    # 选择 k 个 GPU（此处简单取前 k 个，可按需排序）
    allocated_gpus = list(available_gpus.keys())[:k]
    
    # 输出结果
    print(f"分配的 GPU:")
    for gpu in allocated_gpus:
        print(f"GPU {gpu}, 空闲显存={gpu_free_memory[gpu]}")
    
    return allocated_gpus


    
#多目标整数线性规划
def optimize_task_preemption(gpus, tasks, m, k):
    """
    优化任务抢占策略：最小化抢占任务数量并最大化服务量。
    
    参数：
        gpus (list): GPU ID 列表，例如 ["192.168.0.209:0", "192.168.0.209:1"]。
        tasks (dict): 任务字典，格式 {task_id: (gpu_id, mem, svc)}。
        m (int/float): 每个满足条件的 GPU 所需的最小显存。
        k (int): 需要满足条件的 GPU 数量。
    
    返回：
        dict: 包含求解状态、最优任务数量、服务量、抢占任务和满足条件的 GPU。
    """
    prob1 = pulp.LpProblem("Minimize_Task_Count", pulp.LpMinimize)
    x1 = {t: pulp.LpVariable(f"x_{t}", cat="Binary") for t in tasks}
    y1 = {i: pulp.LpVariable(f"y_{i}", cat="Binary") for i in gpus}
    
    prob1 += pulp.lpSum(x1[t] for t in tasks)
    for i in gpus:
        tasks_on_gpu_i = [t for t in tasks if tasks[t][0] == i]
        prob1 += pulp.lpSum(tasks[t][1] * x1[t] for t in tasks_on_gpu_i) >= m * y1[i]
    prob1 += pulp.lpSum(y1[i] for i in gpus) == k
    
    prob1.solve()
    if pulp.LpStatus[prob1.status] != "Optimal":
        return {
            "status": pulp.LpStatus[prob1.status],
            "message": f"第一阶段无最优解，无法找到最小任务数量的可行解，状态: {pulp.LpStatus[prob1.status]}"
        }
    
    n_min = int(pulp.value(prob1.objective))

    prob2 = pulp.LpProblem("Maximize_Service_Quantity", pulp.LpMaximize)
    x2 = {t: pulp.LpVariable(f"x_{t}", cat="Binary") for t in tasks}
    y2 = {i: pulp.LpVariable(f"y_{i}", cat="Binary") for i in gpus}
    
    prob2 += pulp.lpSum(tasks[t][2] * x2[t] for t in tasks)
    for i in gpus:
        tasks_on_gpu_i = [t for t in tasks if tasks[t][0] == i]
        prob2 += pulp.lpSum(tasks[t][1] * x2[t] for t in tasks_on_gpu_i) >= m * y2[i]
    prob2 += pulp.lpSum(y2[i] for i in gpus) == k
    prob2 += pulp.lpSum(x2[t] for t in tasks) == n_min
    
    prob2.solve()
    
    if pulp.LpStatus[prob2.status] != "Optimal":
        return {
            "status": pulp.LpStatus[prob2.status],
            "message": f"第二阶段无最优解，无法在任务数量 {n_min} 下最大化服务量，状态: {pulp.LpStatus[prob2.status]}"
        }

    result = {
        "status": "Optimal",
        "min_task_count": n_min,
        "max_service_quantity": pulp.value(prob2.objective),
        "preempted_tasks": [],
        "satisfied_gpus": []
    }
    
    for t in tasks:
        if x2[t].value() > 0.5:
            result["preempted_tasks"].append({
                "task_id": t,
                "gpu_id": tasks[t][0],
                "memory": tasks[t][1],
                "service": tasks[t][2]
            })
    
    for i in gpus:
        if y2[i].value() > 0.5:
            released = sum(tasks[t][1] * x2[t].value() for t in tasks if tasks[t][0] == i)
            result["satisfied_gpus"].append({
                "gpu_id": i,
                "released_memory": released
            })
    
    return result
