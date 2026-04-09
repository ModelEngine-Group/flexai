#!/bin/bash
# Copyright (C) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.

set -e

var_log_xpu_path="/var/log/xpu"
namespace="xpu"
deployment_name="gpupool"
https_arg=""
monitor_arg=""

print_help() {
    echo "用法: $0 [-c][-h|--help]"
    echo " -c, --https 启用https服务"
    echo " -h, --help  显示此帮助信息"
}

while [[ "$#" -gt 0 ]]; do 
    case $1 in
        -c|--https)
            modify=1
            shift
            ;;
        -h|--help)
            print_help
            exit 0
            ;;
        *)
            echo "未知参数: $1"
            print_help
            exit 1
            ;;
    esac
done 

check_dir() {
    local dir="${1}"
    if [ -e "${dir}" ]; then
        if [ "$(stat -c %a ${dir})" != "750" ]; then
            chmod 750 ${dir}
        fi
    else
        mkdir -m 750 -p ${dir}
    fi
}

# 检查目录是否存在以及权限是否正确
check_dir "${var_log_xpu_path}"

#检查helm是否安装
if command -v helm &> /dev/null; then
    echo "Helm is installed."
else
    echo "Helm not found, XPU pooling software installation failed."
    exit 1
fi

#检查节点环境
if command -v nvidia-smi &> /dev/null; then
    echo "Currently in GPU environment."
    env="gpu"
    images_path="./images/gpupool_x86.tar"
elif ls /dev/davinci* &> /dev/null; then
    echo "Currently in NPU environment."
    env="npu"
    images_path="./images/npupool_x86.tar"
else
    echo "GPU or NPU environment unavailable."
    exit 1
fi


# 根据环境导入对应镜像
if command -v crictl &> /dev/null && command -v ctr &> /dev/null; then
    ctr -n=k8s.io i import ${images_path}
elif command -v docker &> /dev/null; then
    docker load -i ${images_path}
    if [ "${env}" == "gpu" ]; then
        tar -xzvf templates/gpupool-0.1.0.tgz -C templates/ > /dev/null 
        awk '{gsub(/runtimeClassName: nvidia/, "runtimeClassName:"); print}' templates/gpupool/values.yaml > temp.yaml && mv temp.yaml templates/gpupool/values.yaml
        helm package templates/gpupool --destination templates/
    fi
else
    echo "Mirror repository does not exist"
    exit 1
fi

#创建命名空间
if ! kubectl get namespace "$namespace" &> /dev/null 2>&1; then
    ( kubectl create namespace "$namespace" && kubectl label namespace "$namespace" pod-security.kubernetes.io/enforce=privileged ) \
    || { echo "Failed to create k8s namespace xpu"; exit 1; }
fi

# 设置安装参数
function set_args() {
    if [ "$modify" = "1" ]; then
        https_arg="--set xpuExporter.https='"on"'"
    fi
    if kubectl get crds | grep servicemonitors.monitoring.coreos.com > /dev/null; then
        monitor_arg="--set ServiceMonitor.enable="true""
    fi
}

# 安装部署
set_args
helm upgrade --install gpupool ./templates/gpupool-0.1.0.tgz ${https_arg} ${monitor_arg} --wait

# 检查是否部署gpupool
if ! helm list | grep ${deployment_name} > /dev/null; then
    echo "Release '${deployment_name}' not found. Installation failed."
    exit 1
fi

# 获取gpupool的状态并检查是否为deployed
release_status=$(helm status "${deployment_name}" | grep STATUS | awk '{print $2}')
if [[ $release_status = "deployed" ]]; then
    echo "Release '${deployment_name}' installed successfully"
else
    echo "The deployment status of release '${deployment_name}' is not deployed. Current state is ${release_status}"
    exit 1
fi

# 检查XPU池化组件的状态是否都已经Ready
pads_name=$(kubectl get pod -n "${namespace}" | tail -n +2 | awk '{print $1}')
for pod in ${pads_name}; do
    kubectl wait --for=condition=Ready pod/${pod} -n "${namespace}" --timeout=60s
    if [ $? -ne 0 ]; then
        echo "XPU pooling software have some pods that are not in Ready state"
        exit 1
    fi
done

echo "Installation completed!"