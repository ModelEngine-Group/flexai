import logging
import os
import subprocess
import time
from datetime import datetime
from multiprocessing import Process


logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    datefmt="%Y-%m-%d %H:%M:%S",
)


TRACE_30 = [
    (1001, 1, 0, "mobilenet_v2", 0, 32, 0, 0.05, 1),
    (1002, 1, 0, "GPT3", 0, 2, 1, 0.8, 4),
    (1003, 1, 0, "resnet50", 0, 32, 0, 0.3, 4),
    (1004, 1, 0, "Bert", 0, 64, 2, 0.3, 4),
    200,
    (1005, 1, 0, "shufflenet_v2_x0_5", 0, 32, 0, 0.05, 1),
    (1006, 1, 0, "resnet18", 0, 64, 0, 0.3, 4),
    (1007, 1, 0, "GPT3", 0, 2, 1, 0.8, 2),
    (1008, 1, 0, "mobilenet_v2", 0, 32, 0, 0.05, 1),
    200,
    (1009, 1, 0, "Bert", 0, 64, 2, 0.3, 4),
    (1010, 1, 0, "vgg16", 0, 64, 0, 0.3, 4),
    (1011, 1, 0, "shufflenet_v2_x0_5", 0, 32, 0, 0.05, 1),
    (1012, 1, 0, "resnet18", 0, 32, 0, 0.05, 1),
    200,
    (1013, 1, 0, "GPT3", 0, 2, 1, 0.8, 12),
    (1014, 1, 0, "mobilenet_v2", 0, 32, 0, 0.05, 1),
    (1015, 1, 0, "Bert", 0, 64, 2, 0.3, 4),
    (1016, 1, 0, "resnet50", 0, 64, 0, 0.3, 4),
    200,
    (1017, 1, 0, "shufflenet_v2_x0_5", 0, 32, 0, 0.05, 1),
    (1018, 1, 0, "GPT3", 0, 2, 1, 0.05, 1),
    (1019, 1, 0, "vgg16", 0, 32, 0, 0.05, 1),
    (1020, 1, 0, "mobilenet_v2", 0, 32, 0, 0.05, 1),
    200,
    (1021, 1, 0, "Bert", 0, 64, 2, 0.3, 4),
    (1022, 1, 0, "resnet50", 0, 32, 0, 0.05, 1),
    (1023, 1, 0, "GPT3", 0, 2, 1, 0.05, 1),
    (1024, 1, 0, "shufflenet_v2_x0_5", 0, 32, 0, 0.05, 1),
    200,
    (1025, 1, 0, "resnet18", 0, 32, 0, 0.05, 1),
    (1026, 1, 0, "mobilenet_v2", 0, 32, 0, 0.05, 1),
    (1027, 1, 0, "Bert", 0, 32, 2, 0.05, 1),
    (1028, 1, 0, "resnet50", 0, 64, 0, 0.3, 4),
    200,
    (1029, 1, 0, "GPT3", 0, 2, 1, 0.05, 1),
    (1030, 1, 0, "vgg16", 0, 32, 0, 0.05, 1),
]


def run_command(command_str, log_file_path):
    with open(log_file_path, "w", encoding="utf-8") as log_file:
        subprocess.run(command_str, shell=True, stdout=log_file, stderr=log_file)


def handle_request(
    client_id,
    req_gpu_num,
    priority,
    model,
    request_type,
    batchsize,
    job_type,
    batch_rate,
    epoch,
    imagenet_root,
    url=0,
):
    cv_command = [
        f"FLEXGV_CLIENT_ID={client_id}",
        f"FLEXGV_PRIORITY={priority}",
        f"FLEXGV_REQ_NUM={req_gpu_num}",
        f"FLEXGV_MODEL={model}",
        f"FLEXGV_BATCH_SIZE={batchsize}",
        "LD_PRELOAD=./out/lib64/libcuda_hook.so",
        "LD_LIBRARY_PATH=./out/lib64:$LD_LIBRARY_PATH",
        "python",
        "scripts/workloads/imageNetTrain.py",
        "-a",
        model,
        "-b",
        str(batchsize),
        "--gpu",
        "0",
        "-j",
        "2",
        "--epochs",
        str(epoch),
    ]
    if request_type == 1:
        cv_command.extend(["--evaluate", "--pretrained"])
    elif request_type == 0 and batch_rate > 0:
        cv_command.extend(["--batch_rate", f"{batch_rate}"])

    gpt_command = [
        f"FLEXGV_CLIENT_ID={client_id}",
        f"FLEXGV_PRIORITY={priority}",
        f"FLEXGV_REQ_NUM={req_gpu_num}",
        "FLEXGV_MODEL=gpt",
        f"FLEXGV_BATCH_SIZE={batchsize}",
        "LD_PRELOAD=./out/lib64/libcuda_hook.so",
        "LD_LIBRARY_PATH=./out/lib64:$LD_LIBRARY_PATH",
        "python",
        "scripts/workloads/large-language-model/clmTrainWoPrep.py",
        "-b",
        str(batchsize),
        "-e",
        str(epoch),
    ]

    bert_command = [
        f"FLEXGV_CLIENT_ID={client_id}",
        f"FLEXGV_PRIORITY={priority}",
        f"FLEXGV_REQ_NUM={req_gpu_num}",
        "FLEXGV_MODEL=BERT",
        f"FLEXGV_BATCH_SIZE={batchsize}",
        "LD_PRELOAD=./out/lib64/libcuda_hook.so",
        "LD_LIBRARY_PATH=./out/lib64:$LD_LIBRARY_PATH",
        "python",
        "scripts/workloads/text-classification/glueTrainWoPrep.py",
        "-b",
        str(batchsize),
        "-e",
        str(epoch),
    ]

    ddp_command = [
        f"FLEXGV_CLIENT_ID={client_id}",
        f"FLEXGV_PRIORITY={priority}",
        f"FLEXGV_REQ_NUM={req_gpu_num}",
        f"FLEXGV_MODEL={model}",
        f"FLEXGV_BATCH_SIZE={batchsize}",
        "LD_PRELOAD=./out/lib64/libcuda_hook.so",
        "LD_LIBRARY_PATH=./out/lib64:$LD_LIBRARY_PATH",
        "python",
        "scripts/workloads/imageNetTrainDDP.py",
        imagenet_root,
        "-a",
        model,
        "-b",
        str(batchsize),
        "--epochs",
        str(epoch),
        "--dist-url",
        f"tcp://127.0.0.1:166{url}",
    ]
    if request_type == 1:
        ddp_command.extend(["--evaluate", "--pretrained"])
    elif request_type == 0 and batch_rate > 0:
        ddp_command.extend(["--batch_rate", f"{batch_rate}"])

    commands = {
        0: cv_command,
        1: gpt_command,
        2: bert_command,
        3: ddp_command,
    }
    command_str = " ".join(commands[job_type])
    logging.info("Executing command: %s", command_str)

    result_dir = f"result_{datetime.now().strftime('%Y-%m-%d')}"
    os.makedirs(result_dir, exist_ok=True)
    log_file_path = os.path.join(result_dir, f"output{client_id}.log")

    process = Process(target=run_command, args=(command_str, log_file_path))
    process.start()
    return process


def run_trace(trace, imagenet_root):
    for item in trace:
        if isinstance(item, (int, float)):
            time.sleep(item)
            continue
        handle_request(*item, imagenet_root=imagenet_root)

