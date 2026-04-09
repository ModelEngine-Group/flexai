#!/bin/bash
# Copyright (C) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.

set -e 

work_path="/opt/xpu"
var_log_xpu_path="/var/log/xpu"
var_lib_xpu_path="/var/lib/xpu"
etc_xpu_path="/etc/xpu"
work_lib_path=${work_path}/lib
lib_path="/usr/lib64"
namespace="xpu"
cuda_path=${lib_path}/libcuda.so
cuda_original_path=${work_lib_path}/libcuda-original.so
deployment_name="gpupool"
RED='\033[0;31m'
NC='\033[0m'

#获取池化组件相关pod
pods=$(kubectl get pod -n "${namespace}" | tail -n +2 | awk '{print $1}')

#卸载池化组件
if helm list --deployed --short | grep -q ${deployment_name}; then
    helm uninstall gpupool
else 
    echo "The helm of gpupool has been uninstalled. Next, we will clean up the remaining residues"
fi

# 检查XPU池化组件的状态是否都已经卸载
pod_delete_flag="true"
echo "Wait for Pods to be deleted."
for pod in ${pods}; do
    kubectl wait --for=delete pod/${pod} -n "${namespace}" --timeout=120s 2>/dev/null  || status=$? || true
    if [ ${status} -ne 0 ]; then
        echo "'${pod}' deletion timeout"
        pod_delete_flag="false"
    fi
done

#恢复libcuda.so文件
cuda_file=$(readlink -f ${cuda_path})
cuda_name=$(basename "${cuda_file}")
cuda_backup_name=$(find ${work_lib_path} -name ${cuda_name}.bak )
if [ "${cuda_backup_name}" = "" ]; then
    echo -e "The corresponding cuda lib file was not found in the ${work_lib_path} folder"
    exit 1
fi
install -m 755 ${cuda_backup_name} ${cuda_file}

#提示xpu相关文件存留
echo "The following XPU related folders will be retained."
echo ${work_path}
echo ${var_log_xpu_path}
echo ${var_lib_xpu_path}
echo ${etc_xpu_path}

if [ ${pod_delete_flag} = "false" ]; then
    echo -e "${RED}Some pods have not been deleted.${NC}"
    echo -e "${RED}You are advised to manually delete the pod and run the script again to clear residual data.${NC}"
    exit 1
fi

echo "Uninstall gpupool completed!"
