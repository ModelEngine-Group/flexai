import random
import time
from tqdm import tqdm
import pulp


#测试单目标求解拓展性
# 生成模拟数据：GPU 和任务
def generate_gpu_tasks(gpu_count, task_per_gpu=2):
    gpus = [f"gpu_{i}" for i in range(gpu_count)]
    gpu_free_memory = {gpu: 10 for gpu in gpus}  # 初始显存 10
    tasks = {}
    task_id = 0
    
    for gpu in gpus:
        num_tasks = random.randint(1, 3)
        for _ in range(num_tasks):
            mem = random.randint(1, 5)  # 任务占用显存 [1, 5]
            gpu_free_memory[gpu] -= mem  # 更新空闲显存
            tasks[task_id] = (gpu, mem, random.randint(1, 10))  # 服务量随机
            task_id += 1
    
    # 确保空闲显存非负
    for gpu in gpus:
        gpu_free_memory[gpu] = max(0, gpu_free_memory[gpu])
    
    return gpus, tasks, gpu_free_memory

# 贪心分配 GPU
def allocate_gpus(gpu_free_memory, m, k):
    start_time = time.time()
    available_gpus = {gpu: free for gpu, free in gpu_free_memory.items() if free >= m}
    
    if len(available_gpus) < k:
        return None, time.time() - start_time
    
    allocated_gpus = list(available_gpus.keys())[:k]
    total_time = time.time() - start_time
    return allocated_gpus, total_time

# 实验主函数
def run_allocation_experiment(new_task_counts=[1, 5, 10]):
    gpu_sizes = [100, 1000, 2500, 5000, 10000]
    m = 5  # 每个 GPU 需提供 5 显存
    k_single = 2  # 每个新任务需要 2 个 GPU
    results = []

    print("开始空闲 GPU 分配扩展性实验...")
    for gpu_count in tqdm(gpu_sizes):
        gpus, tasks, gpu_free_memory = generate_gpu_tasks(gpu_count)
        task_count = len(tasks)
        
        for N in new_task_counts:
            k = N * k_single  # 总 GPU 需求
            if k > gpu_count:
                print(f"\nGPU={gpu_count}, N={N}: k={k} 超出 GPU 总数，跳过")
                continue
            
            allocated_gpus, alloc_time = allocate_gpus(gpu_free_memory, m, k)
            if allocated_gpus is None:
                print(f"\nGPU={gpu_count}, N={N}: 无足够空闲 GPU")
                continue
            
            results.append({
                "GPU Count": gpu_count,
                "Task Count": task_count,
                "New Tasks (N)": N,
                "k": k,
                "Allocation Time (s)": alloc_time
            })
            print(f"\nGPU={gpu_count}, Tasks={task_count}, New Tasks={N}, k={k}:")
            print(f"  Allocation Time={alloc_time:.6f}s")

    # 输出结果表格
    print("\n实验结果汇总：")
    print("| GPU Count | Task Count | New Tasks (N) | k   | Allocation Time (s) |")
    print("|-----------|------------|---------------|-----|---------------------|")
    for res in results:
        print(f"| {res['GPU Count']:<9} | {res['Task Count']:<10} | {res['New Tasks (N)']:<13} | {res['k']:<3} | {res['Allocation Time (s)']:<19.6f} |")

# 运行实验
# if __name__ == "__main__":
#     run_allocation_experiment(new_task_counts=[1, 5, 10])
    
    
    
#测试多目标求解


# import random
# import time
# from tqdm import tqdm  # 用于显示进度条

# 生成模拟数据的函数
def generate_tasks(gpu_count, task_per_gpu=2):
    gpus = [f"gpu_{i}" for i in range(gpu_count)]
    tasks = {}
    task_id = 0
    for gpu in gpus:
        # 每个 GPU 随机 1-3 个任务，平均约 2 个
        num_tasks = random.randint(1, 3)
        for _ in range(num_tasks):
            mem = random.randint(1, 10)  # 显存 [1, 10]
            svc = random.randint(1, 10)  # 服务量 [1, 10]
            tasks[task_id] = (gpu, mem, svc)
            task_id += 1
    return gpus, tasks

# MOILP 求解函数
def solve_moilp(gpus, tasks, m, k):
    # 第一阶段：最小化任务数量
    start_time = time.time()
    prob1 = pulp.LpProblem("Minimize_Task_Count", pulp.LpMinimize)
    x = {t: pulp.LpVariable(f"x_{t}", cat="Binary") for t in tasks}
    y = {i: pulp.LpVariable(f"y_{i}", cat="Binary") for i in gpus}
    prob1 += pulp.lpSum(x[t] for t in tasks)
    for i in gpus:
        tasks_on_gpu_i = [t for t in tasks if tasks[t][0] == i]
        prob1 += pulp.lpSum(tasks[t][1] * x[t] for t in tasks_on_gpu_i) >= m * y[i]
    prob1 += pulp.lpSum(y[i] for i in gpus) == k
    prob1.solve(pulp.PULP_CBC_CMD(msg=0))  # msg=0 关闭求解日志
    stage1_time = time.time() - start_time
    if pulp.LpStatus[prob1.status] != "Optimal":
        return None, stage1_time, 0
    n_min = int(pulp.value(prob1.objective))

    # 第二阶段：最大化服务量
    start_time = time.time()
    prob2 = pulp.LpProblem("Maximize_Service_Quantity", pulp.LpMaximize)
    x = {t: pulp.LpVariable(f"x_{t}", cat="Binary") for t in tasks}
    y = {i: pulp.LpVariable(f"y_{i}", cat="Binary") for i in gpus}
    prob2 += pulp.lpSum(tasks[t][2] * x[t] for t in tasks)
    for i in gpus:
        tasks_on_gpu_i = [t for t in tasks if tasks[t][0] == i]
        prob2 += pulp.lpSum(tasks[t][1] * x[t] for t in tasks_on_gpu_i) >= m * y[i]
    prob2 += pulp.lpSum(y[i] for i in gpus) == k
    prob2 += pulp.lpSum(x[t] for t in tasks) == n_min
    prob2.solve(pulp.PULP_CBC_CMD(msg=0))
    stage2_time = time.time() - start_time

    total_time = stage1_time + stage2_time
    return n_min, stage1_time, stage2_time

# 实验主函数
def run_experiment():
    gpu_sizes = [10, 50, 100, 1000, 2500, 5000, 10000]
    m = 5
    results = []

    print("开始扩展性实验...")
    for gpu_count in tqdm(gpu_sizes):
        # k = int(0.1 * gpu_count)  # 抢占 10% 的 GPU
        k = 2
        gpus, tasks = generate_tasks(gpu_count)
        task_count = len(tasks)
        
        n_min, stage1_time, stage2_time = solve_moilp(gpus, tasks, m, k)
        total_time = stage1_time + stage2_time
        
        results.append({
            "GPU Count": gpu_count,
            "Task Count": task_count,
            "k": k,
            "Min Tasks": n_min,
            "Stage 1 Time (s)": stage1_time,
            "Stage 2 Time (s)": stage2_time,
            "Total Time (s)": total_time
        })
        print(f"\nGPU={gpu_count}, Tasks={task_count}, k={k}:")
        print(f"  Min Tasks={n_min}, Total Time={total_time:.2f}s (Stage 1: {stage1_time:.2f}s, Stage 2: {stage2_time:.2f}s)")

    # 输出结果表格
    print("\n实验结果汇总：")
    print("| GPU Count | Task Count | k   | Min Tasks | Stage 1 Time (s) | Stage 2 Time (s) | Total Time (s) |")
    print("|-----------|------------|-----|-----------|------------------|------------------|----------------|")
    for res in results:
        print(f"| {res['GPU Count']:<9} | {res['Task Count']:<10} | {res['k']:<3} | {res['Min Tasks'] or 'N/A':<9} | {res['Stage 1 Time (s)']:<16.2f} | {res['Stage 2 Time (s)']:<16.2f} | {res['Total Time (s)']:<14.2f} |")

# 运行实验
if __name__ == "__main__":
    run_allocation_experiment(new_task_counts=[1, 5, 10])
    run_experiment()