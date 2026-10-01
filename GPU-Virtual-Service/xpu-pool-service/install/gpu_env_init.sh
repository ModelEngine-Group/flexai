#!/bin/bash
# Copyright (c) 2025 Shanghai Jiao Tong University.
# Contributed to the ModelEngine Community.

set -e

host_name=$(hostname)

#检查是否安装cuda
if command -v nvcc --version &> /dev/null; then
    echo "The CUDA has been installed."
    cuda_version=$(nvcc --version | grep release | awk -F ' ' '{print $5}'| cut -d ',' -f1)
    echo "CUDA version: $cuda_version"
else
    echo "CUDA not found, please ensure CUDA is installed and add environment variables"
    exit 1
fi

#检查是否安装nvidia驱动
if command -v nvidia-smi &> /dev/null; then
    echo "The NVIDIA driver has been installed."
    echo "NVIDIA driver version:"
    nvidia-smi --query-gpu=driver_version --format=csv,noheader | tail -n 1
    echo "GPU card:"
    nvidia-smi --query-gpu=gpu_name --format=csv,noheader
else
    echo "NVIDIA driver not found, please ensure NVIDIA driver is installed."
    exit 1
fi

kubectl label node ${host_name} flexai.com/vgpu=ready