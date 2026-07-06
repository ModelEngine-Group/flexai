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


# 设置日志
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

workload1 = [ #低推理、低训练负载
        f"User_Reqeust_Training:user1,resnet18,{32},{1}",
        f"User_Reqeust_Training:user2,resnet18,{32},{1}",
        # f"User_Reqeust_Training:user3,mobilenet_v2,{32},{1}",     
        # f"wait",
             
        f"User_Request_Inference:user4,resnet18,{50},True,{100},{4}",
        f"User_Request_Inference:user5,vgg16,{40},True,{100},{2}",
        f"User_Request_Inference:user6,densenet121,{100},True,{100},{8}",

]

workload2 = [ #低推理、高训练负载
             
        f"User_Reqeust_Training:user5,resnet50,{64},{1}",
        f"User_Reqeust_Training:user6,resnet50,{32},{1}",
        f"User_Reqeust_Training:user7,resnet50,{64},{1}",
        f"User_Reqeust_Training:user8,resnet50,{32},{1}",
        f"wait",
        f"User_Request_Inference:user1,resnet18,{50},True,{100},{4}",
        f"User_Request_Inference:user2,vgg16,{40},True,{100},{2}",
        f"User_Request_Inference:user3,densenet121,{100},True,{100},{8}",
        f"User_Request_Inference:user4,mobilenet_v2,{50},True,{100},{4}",

        # f"User_Reqeust_Training:user9,resnet50,{128},{1}"
        # f"User_Reqeust_Training:user10,resnet50,{128},{1}"     
]

workload3 = [ #高推理、低训练负载
        # f"User_Reqeust_Training:user01,resnet18,{32},{1}",
        # f"User_Reqeust_Training:user02,resnet18,{32},{1}",
        # f"User_Reqeust_Training:user03,mobilenet_v2,{32},{1}",
        # f"wait",
        f"User_Request_Inference:user1,resnet18,{200},True,{100},{4}",
        f"User_Request_Inference:user2,resnet18,{100},True,{100},{8}",
        f"User_Request_Inference:user3,resnet18,{100},True,{100},{4}",
        f"User_Request_Inference:user4,resnet18,{100},True,{100},{16}",
        f"User_Request_Inference:user5,resnet18,{100},True,{100},{16}",
        f"User_Request_Inference:user6,resnet18,{100},True,{100},{8}",
        f"User_Request_Inference:user7,vgg16,{100},True,{100},{4}",
        f"User_Request_Inference:user8,vgg16,{100},True,{100},{8}",
        f"User_Request_Inference:user9,vgg16,{100},True,{100},{8}",
        f"User_Request_Inference:user10,vgg16,{100},True,{100},{16}",
        f"User_Request_Inference:user11,vgg16,{100},True,{100},{16}",
        f"User_Request_Inference:user12,vgg16,{100},True,{100},{8}",    
        f"User_Request_Inference:user13,densenet121,{100},True,{100},{4}",
        f"User_Request_Inference:user14,densenet121,{100},True,{100},{8}",
        f"User_Request_Inference:user15,densenet121,{100},True,{100},{8}",
        f"User_Request_Inference:user16,densenet121,{100},True,{100},{16}",
        f"User_Request_Inference:user17,densenet121,{100},True,{100},{16}",
        f"User_Request_Inference:user18,densenet121,{100},True,{100},{8}",
]

workload4 = [ #高推理、高训练负载   
        f"User_Reqeust_Training:user05,resnet50,{64},{1}",
        f"User_Reqeust_Training:user06,resnet50,{64},{1}",
        f"User_Reqeust_Training:user07,resnet18,{64},{1}",
        f"User_Reqeust_Training:user08,resnet18,{64},{1}",
        f"User_Reqeust_Training:user09,resnet50,{32},{1}",
        f"User_Reqeust_Training:user010,resnet50,{32},{1}",
        f"wait",
        f"User_Request_Inference:user1,resnet18,{200},True,{100},{4}",
        f"User_Request_Inference:user2,resnet18,{100},True,{100},{8}",
        f"User_Request_Inference:user3,resnet18,{100},True,{100},{4}",
        f"User_Request_Inference:user4,resnet18,{100},True,{100},{16}",
        f"User_Request_Inference:user5,resnet18,{100},True,{100},{16}",
        f"User_Request_Inference:user6,resnet18,{100},True,{100},{8}",
        f"User_Request_Inference:user7,vgg16,{100},True,{100},{4}",
        f"User_Request_Inference:user8,vgg16,{100},True,{100},{8}",
        f"User_Request_Inference:user9,vgg16,{100},True,{100},{8}",
        f"User_Request_Inference:user10,vgg16,{100},True,{100},{16}",
        f"User_Request_Inference:user11,vgg16,{100},True,{100},{16}",
        f"User_Request_Inference:user12,vgg16,{100},True,{100},{8}",    
        f"User_Request_Inference:user13,densenet121,{100},True,{100},{4}",
        f"User_Request_Inference:user14,densenet121,{100},True,{100},{8}",
        f"User_Request_Inference:user15,densenet121,{100},True,{100},{8}",
        f"User_Request_Inference:user16,densenet121,{100},True,{100},{16}",
        f"User_Request_Inference:user17,densenet121,{100},True,{100},{16}",
        f"User_Request_Inference:user18,densenet121,{100},True,{100},{8}",
    

]


workload5 = [ #高推理、低训练负载，推理使用泊松分布
        # f"User_Reqeust_Training:user04,resnet18,{8},{1}",
        # f"User_Reqeust_Training:user05,resnet18,{8},{1}",
        # f"User_Reqeust_Training:user06,mobilenet_v2,{32},{1}"   
        f"wait", 
        f"User_Request_Inference:user1,resnet18,{200},False,{100},{4}",
        f"User_Request_Inference:user2,resnet18,{100},False,{100},{8}",
        f"User_Request_Inference:user3,resnet18,{100},False,{100},{4}",
        f"User_Request_Inference:user4,resnet18,{100},False,{100},{16}",
        f"User_Request_Inference:user5,resnet18,{100},False,{100},{16}",
        f"User_Request_Inference:user6,resnet18,{100},False,{100},{8}",
        f"User_Request_Inference:user7,vgg16,{100},False,{100},{4}",
        f"User_Request_Inference:user8,vgg16,{100},False,{100},{8}",
        f"User_Request_Inference:user9,vgg16,{100},False,{100},{8}",
        f"User_Request_Inference:user10,vgg16,{100},False,{100},{16}",
        f"User_Request_Inference:user11,vgg16,{100},False,{100},{16}",
        f"User_Request_Inference:user12,vgg16,{100},False,{100},{8}",    
        f"User_Request_Inference:user13,densenet121,{100},False,{100},{4}",
        f"User_Request_Inference:user14,densenet121,{100},False,{100},{8}",
        f"User_Request_Inference:user15,densenet121,{100},False,{100},{8}",
        f"User_Request_Inference:user16,densenet121,{100},False,{100},{16}",
        f"User_Request_Inference:user17,densenet121,{100},False,{100},{16}",
        f"User_Request_Inference:user18,densenet121,{100},False,{100},{8}",
 
    ]


def main():
    config = get_config_file()
    glb_Ip = config["GlobalConfig"]["glbIp_"]
    glb_Port = config["GlobalConfig"]["glbPort_"]
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((glb_Ip, glb_Port))
    logger.info(f"Connected to Global Server at {glb_Ip}:{glb_Port}")
    # 发送请求
    
    for msg in workload2:
        if msg == "wait":
                time.sleep(20)
                continue
        logger.info(f"Sending message: {msg}")
        s.sendall(msg.encode())
        time.sleep(0.05)
    
        
    while True:
        time.sleep(1)
        
if __name__ == "__main__":
    main()
    