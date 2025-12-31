#!/bin/bash
# CUDA 客户端更新脚本
# 功能：初始化 CUDA 库文件替换，并持续监控和更新相关文件
# 说明：用于在容器或系统中安装和更新 GPU 监控工具和 CUDA 库包装器

# 定义根路径和工作路径
root_path="/root"              # 文件源路径，存放更新文件的根目录
work_path="/opt/xpu"           # 工作路径，XPU 工具的安装目录
work_lib_path=${work_path}/lib # 库文件安装路径
work_bin_path=${work_path}/bin # 可执行文件安装路径
lib_path="/usr/lib64"          # 系统 CUDA 库路径
direct_name="libcuda_direct.so"    # CUDA 库包装器文件名
monitor_name="gpu-monitor"         # GPU 监控工具文件名
monitor_link="xpu-monitor"         # 监控工具的符号链接名称
tool_name="xpu-client-tool"        # 客户端工具文件名

# 创建工作目录，设置权限为 555（读和执行权限）
mkdir -m 555 -p ${work_lib_path}
mkdir -m 555 -p ${work_bin_path}

# 定义文件路径变量
cuda_original_path=${work_lib_path}/libcuda-original.so  # 原始 CUDA 库备份路径
monitor_path=${work_bin_path}/${monitor_name}            # 监控工具安装路径
monitor_linkpath=${work_bin_path}/$monitor_link        # 监控工具符号链接路径
tool_path=${work_bin_path}/${tool_name}                  # 客户端工具安装路径
root_direct_path=${root_path}/${direct_name}             # 根目录下的 CUDA 包装器路径
root_monitor_path=${root_path}/${monitor_name}           # 根目录下的监控工具路径
root_tool_path=${root_path}/${tool_name}                 # 根目录下的客户端工具路径

# 安装监控工具和客户端工具到工作目录，设置权限为 555
install -m 555 ${root_monitor_path} ${monitor_path}
install -m 555 ${root_tool_path} ${tool_path}
# 创建监控工具的符号链接（从 xpu-monitor 指向 gpu-monitor）
ln -fs $monitor_name $monitor_linkpath

# 获取系统 CUDA 库文件的真实路径（处理符号链接）
cuda_file=$(readlink -f ${lib_path}/libcuda.so)
# 提取 CUDA 库文件名（去除路径）
cuda_name=$(basename "${cuda_file}")
# 定义 CUDA 库备份路径（在工作目录和容器根目录）
cuda_backup_path=${work_lib_path}/${cuda_name}.bak
cuda_container_backup_path=${root_path}/${cuda_name}.bak

# CUDA 库文件替换逻辑
if [ "${cuda_file}" == "" ]; then
  # CUDA 工具包未安装，退出脚本
  echo "cuda toolkit not installed"
  exit 1
elif [ ! -f ${cuda_backup_path} ]; then
  # 首次初始化：备份原始 CUDA 库并替换为包装器
  # 1. 备份原始库到工作目录（权限 555，可执行）
  # 2. 备份原始库到工作目录的备份文件（权限 400，只读）
  # 3. 备份原始库到容器根目录（权限 400，只读）
  # 4. 用包装器替换系统库（权限 555，可执行）
  install -m 555 "${cuda_file}" "${cuda_original_path}" \
    && install -m 400 "${cuda_file}" "${cuda_backup_path}" \
    && install -m 400 "${cuda_file}" "${cuda_container_backup_path}" \
    && install -m 555 "${root_direct_path}" "${cuda_file}" \
    && echo "client file initialization completed" || (echo "client file initialization failed" && exit 1)
else
  # 已初始化过：更新容器内的备份文件
  install -m 400 "${cuda_backup_path}" "${cuda_container_backup_path}" \
    || echo "client file in-container backup failed"
fi

# update 函数：比较文件 SHA256 哈希值，如果不同则更新文件
# 参数：$1 - 源文件路径，$2 - 目标文件路径，$3 - 文件描述（用于日志）
update() {
  # 计算源文件和目标文件的 SHA256 哈希值
  src_sha256=$(sha256sum "${1}" | awk '{ print $1 }')
  dest_sha256=$(sha256sum "${2}" | awk '{ print $1 }')
  # 如果哈希值不同，说明文件已更新，需要复制新文件
  if [ "${src_sha256}" != "${dest_sha256}" ]; then
    /bin/cp -f "${1}" "${2}"
    echo "${3} file has been updated"
  fi
}

# update_install 函数：比较文件 SHA256 哈希值，如果不同则安装文件（带权限设置）
# 参数：$1 - 源文件路径，$2 - 目标文件路径，$3 - 文件描述（用于日志）
update_install() {
  # 计算源文件和目标文件的 SHA256 哈希值
  src_sha256=$(sha256sum "${1}" | awk '{ print $1 }')
  dest_sha256=$(sha256sum "${2}" | awk '{ print $1 }')
  # 如果哈希值不同，说明文件已更新，需要安装新文件（权限 555）
  if [ "${src_sha256}" != "${dest_sha256}" ]; then
    install -m 555 "${1}" "${2}"
    echo "${3} file has been updated"
  fi
}

# 主循环：持续监控文件更新
while true; do
  # 更新 CUDA 包装器（direct）文件
  update "${root_direct_path}" "${cuda_file}" "direct"
  # 更新监控工具文件
  update "${root_monitor_path}" "${monitor_path}" "monitor"
  # 更新客户端工具文件
  update "${root_tool_path}" "${tool_path}" "client tool"
  
  # 如果容器内备份文件存在，更新容器内的 CUDA 库备份
  if [ -f "${cuda_container_backup_path}" ]; then
    update "${cuda_container_backup_path}" "${cuda_backup_path}" "cuda lib backup"
  fi
  update_install "${cuda_backup_path}" "${cuda_original_path}" "cuda lib"
  
  # 检查监控工具的符号链接是否正确，如果不正确则恢复
  if [ "$(readlink ${monitor_linkpath})" != "${monitor_link}" ]; then
    echo "$monitor_link is restored"
    ln -fs $monitor_name $monitor_linkpath
  fi
  
  # 休眠 5 秒后继续监控
  sleep 5
  echo "files is being monitored"
done
