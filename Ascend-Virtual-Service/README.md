# Flex:ai Ascend 算力切分

## 概述

Flex:ai 在昇腾（Ascend）原生架构基础上，通过深度定制开发，实现了针对国产 AI 算力的高效管理与灵活切分，其核心能力聚焦于 vNPU 虚拟化 与 高密度分时调度： 

1、昇腾算力虚拟化 

该能力旨在打破物理芯片的边界，提升单卡利用率。通过对接昇腾底层驱动与 CANN（芯片算力引擎），Flex:ai 实现了： 硬件级资源隔离： 支持将单张昇腾物理 NPU（如 Ascend 910）虚拟化为多个不同规格的 vNPU 实例，确保业务互不干扰； 动态切分能力： 允许用户根据任务负载，按需配置虚拟卡的算力比例（如 1:1、2:8 等），实现单芯片在多个 AI 推理或小型训练容器间的分时共享。 

2、精细化分时调度 

在虚拟化隔离的基础上，Flex:ai 针对昇腾平台引入了高级调度算法，进一步压榨剩余算力： 时间片轮转复用： 突破了物理切分的数量限制，支持多个 AI 任务在同一个 vNPU 或物理 NPU 上进行毫秒级的时间片轮转。这使得低负载的推理任务能够以“排队抢占”的方式共享同一块算力资源，极大地提升了集群的任务承载密度； 算力保障与优先级： 提供多级调度策略，支持对核心业务设置高优先级，确保在分时复用场景下，关键任务能够优先获得算力响应，降低长尾延迟。

## 硬件平台 

CPU：HUAWEI Kunpeng 920 5250 

NPU：Ascend 910B3 NPU * 1 

## 软件环境 

推荐：使用 vllm-ascend 华为官方镜像 0.8.5rc1 进行配置 

1、安装 vllm-ascend 华为官方镜像 

```
# Update the vllm-ascend image 
# openEuler: 
# export IMAGE=quay.io/ascend/vllm-ascend:v0.8.5rc1-openeuler 
# Ubuntu: 
# export IMAGE=quay.io/ascend/vllm-ascend:v0.8.5rc1 
export IMAGE=quay.io/ascend/vllm-ascend:v0.8.5rc1 

# Run the container using the defined variables 
# Note if you are running bridge network with docker, Please expose available ports 
# for multiple nodes communication in advance 
docker run --rm \
--name vllm-ascend \
--net=host \
--shm-size=512g \
--device /dev/davinci0 \
--device /dev/davinci1 \
--device /dev/davinci2 \
--device /dev/davinci3 \
--device /dev/davinci4 \
--device /dev/davinci5 \
--device /dev/davinci6 \
--device /dev/davinci7 \
--device /dev/davinci_manager \
--device /dev/devmm_svm \
--device /dev/hisi_hdc \
-v /usr/local/dcmi:/usr/local/dcmi \
-v /usr/local/Ascend/driver/tools/hccn_tool:/usr/local/Ascend/driver/tools/hccn_tool \
-v /usr/local/bin/npu-smi:/usr/local/bin/npu-smi \
-v /usr/local/Ascend/driver/lib64/:/usr/local/Ascend/driver/lib64/ \
-v /usr/local/Ascend/driver/version.info:/usr/local/Ascend/driver/version.info \
-v /etc/ascend_install.info:/etc/ascend_install.info \
-v /root/.cache:/root/.cache \ 
-it $IMAGE bash 
```

2、进入 docker 镜像 

`docker exec -it vllm-ascend /bin/bash` 

3、确认 vllm-ascend 已成功安装 

`vllm bench throughput --tensor-parallel-size 1 --gpu-memory-utilization=0.9 --model  /xxx --num-prompts xxx--max-num-seqs xxx--input-len xxx--output-len xxx --seed xxx --max-model-len xxx`

（其中model路径与相关参数需自行添加）

4、（可选）若镜像中cann版本不同，需要于镜像中重新安装cann-toolkit 8.1.rc1

输入 `ls -al /usr/local/Ascend/ascend-toolkit/` 确认 cann 安装版本 

具体安装与下载链接，参见华为官方文档： https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/81RC1beta1/softwareinst/instg/instg_0008.html?Mode=PmIns&InstallType=local&OS=Ubuntu&Software=cannNNAE

## 启动flexnpu-daemon

进入 hypervisor 目录：

1、运行 `./clear.sh` 清理运行缓存

2、运行 `./flexnpud`

查看日志 `./flexnpud.log`

3、运行 `./register.sh` 注册 flexnpu

注册后会在ins目录下生成对应的flexnpu文件，当前需要在重启flexnpud前调用 `./clear.sh` 删除

4、配置说明

配置文件 `./flexnpud.conf` 决定了调度服务的运行环境与核心性能表现，修改时请参考以下说明：
`log_file`：指定服务运行日志的存储路径，用于记录调度过程中的状态信息、任务注册记录以及可能出现的异常报错，便于后续运维排查。

`persistent_file_dir`：指定运行时（Runtime）相关文件的持久化存储目录。系统会将 Flex:ai 管理昇腾算力时的中间状态、元数据及上下文信息保存在此。

`quota`：算子下发的配额参数（取值范围 0 < quota <= 16）。它定义了在每一轮调度循环中，系统允许向 NPU 下发算子的最大次数。该值越大，单次分配获取的计算密度越高，通常需要根据业务的实时性要求进行微调。
其余配置暂未开放支持。

5、`register.sh` 脚本说明
`register.sh` 脚本中的每一条 curl 请求都对应一个需要被调度的用户进程。在实际部署时，请根据以下说明修改参数： 

`npu`：指定进程所绑定的物理芯片编号（如 0、1 等）。需确保该参数与 vLLM 实例实际运行的 NPU 硬件位置一致。 

`aicore_alloc`：定义该进程在分时调度中占据的算力权重（时间片比例）。 需确保同一张 NPU 卡上所有注册NPU任务的 aicore_alloc 总和不超过上文配置中预设的 quota（通常为 10）。

## 体验算力切效果

**进入 aclserver 目录**

1、设置`server.sh`和`server_2.sh`：

设置npu卡号（ASCEND_RT_VISIBLE_DEVICES）；

每个模型占用的显存（CRONUS_DEVPTR_MEMORY_SIZE）；

其余配置暂未开放支持。

2、配置`client.sh`（修改vllm模型路径）

3、清理缓存 `./stopall.sh`

4、启动第一个推理进程

运行`./server.sh 2`，在另一个窗口运行`./client vllm`

5、启动第二个推理进程

当第一个vllm运行到加载权重时，开启第三个窗口运行`./server_2.sh 2`，然后开启第四个窗口运行`./client.sh vllm`

**效果展示**

通过`vllm bench`的throughput结果可观察到算力切分