# Flex:ai 介绍及使用说明  

## 概述

Flex:ai是一个面向AI容器场景的开源项目，其核心能力包含两大部分，分别是XPU虚拟化和多级智能调度。其中XPU虚拟化分为本地XPU虚拟化和跨节点拉远虚拟化，本地XPU虚拟化支持将1台服务器上的XPU算力卡虚拟化分割出多个虚拟算力单元，实现单张算力卡在多个容器的共享。跨节点拉远虚拟化支持通过RDMA或TCP网络访问远端节点上的算力卡。多级智能调度支持对切分出的虚拟卡进行Binpack等资源级调度，以及对AI训推任务进行分时调度等能力。

## 动机
用户的AI业务集群通常是大小模型混部的场景，集群除了运行AI大模型如Deepseek、Qwen系列，还有大量的小参数量模型，如CV模型、Embedding模型等，这些小参数量模型无法充分使用整张算力卡的资源，从而导致昂贵的GPU/NPU资源存在浪费。此外算力集群中运行的AI工作负载千差万别，如何基于有限的算力资源对大并发的AI工作负载进行高效调度是一个难题。基于上述的背景，我们构筑了Flex:ai开源项目，提供将XPU算力卡进行虚拟化切分，以及面向AI训推任务和集群资源做智能调度的能力。

## 试用安装说明
### 1、源码编译
#### cuda劫持

1. 安装cuda：
```bash
wget https://developer.download.nvidia.com/compute/cuda/12.2.0/local_installers/cuda_12.2.0_535.54.03_linux.run
sudo sh cuda_12.2.0_535.54.03_linux.run
```

2. 安装依赖项

下载 [https://github.com/gabime/spdlog/archive/refs/tags/v1.12.0.zip](https://github.com/gabime/spdlog/archive/refs/tags/v1.12.0.zip) 到本地，执行：

```Bash
mkdir -p /usr/local/include
unzip spdlog*.zip && cd spdlog-1.12.0 && cp -rf include/spdlog /usr/local/include
```

下载 [https://github.com/NixOS/patchelf/archive/refs/tags/0.18.0.zip](https://github.com/NixOS/patchelf/archive/refs/tags/0.18.0.zip) 到本地，执行：

```Bash
unzip patchelf-0.18.0.zip && cd patchelf-0.18.0
yum install autoconf automake libtool
./bootstrap.sh
./configure
make
sudo make install
```

3. 编译

下载 [https://github.com/Check4068/Flex-AI/archive/refs/heads/main.zip](https://github.com/Check4068/Flex-AI/archive/refs/heads/main.zip) 到本地，执行：

```Bash
unzip Flex-AI-main.zip && cd Flex-AI-main/xpu-pool-service/
cd direct && chmod +x make_lib_original.sh && cd -
mkdir build && cd build && cmake .. && make -j
```

编译后生成的动态库文件在`direct/cuda`目录下，为`libcuda_direct.so`。

### 设备插件

go的版本为1.25.0，建议保持一致：

```Bash
export CGO_ENABLED=1
cd Flex-AI-main/GPU-device-plugin && go mod tidy && make
```

### 调度组件

下载 [https://github.com/volcano-sh/volcano/archive/refs/tags/v1.10.2.zip](https://github.com/volcano-sh/volcano/archive/refs/tags/v1.10.2.zip) 到本地，执行：

```Bash
unzip volcano-1.10.2.zip
cd $GOPATH
mkdir -p src/volcano.sh/volcano
```

将解压后的`volcano-1.10.2.zip`复制到创建出的目录下：

```Bash
cp -rf volcano-1.10.2/volcano-1.10.2/* src/volcano.sh/volcano
```

将`xpu-scheduler-plugin`目录整个复制到volcano的plugins目录下：

```Bash
cd src/volcano.sh/volcano
cp -rf {path/to/Flex-AI}/pod-scheduler-service/xpu-scheduler-plugin pkg/scheduler/plugins/
```
`{path/to/Flex-AI}`应被替换为项目代码实际所在的目录

执行编译：

```Bash
cd pkg/scheduler/plugins/xpu-scheduler-plugin/build && chmod +x build.sh && ./build.sh
```

在`out`目录下可见编译后产出的文件：`huawei-xpu.so`、`vc-scheduler`、`vc-controller-manager`。

## 2、打包镜像

将编译后产出的文件复制到对应的目录下，在项目的根目录下执行：

```Bash
cp -rf xpu-pool-service/build/direct/cuda/libcuda_direct.so xpu-pool-service/docker-build/client-update
cp -rf xpu-pool-service/build/direct/cuda/gpu-monitor xpu-pool-service/docker-build/client-update
chmod +x xpu-pool-service/client_update/cuda-client-update.sh && cp -rf xpu-pool-service/client_update/cuda-client-update.sh xpu-pool-service/docker-build/client-update
cp -rf xpu-pool-service/GPU-device-plugin/xpu-client-tool xpu-pool-service/docker-build/client-update
```

其中除`cuda-client-update.sh`为脚本文件，剩下的均为编译结果

```Bash
cp -rf xpu-pool-service/GPU-device-plugin/gpu-device-plugin xpu-pool-service/docker-build/gpu-device-plugin
```

在`docker-build/client-update`目录下执行：
```Bash
docker build -t cuda-client-update:v1.0 .
```

在`docker-build/gpu-device-plugin`目录下执行：

```Bash
docker build -t gpu-device-plugin:v1.0 .
```
（上述代码的`.`不能忽略）

至此，镜像制作完成，可以使用如下命令将镜像保存到本地：
```Bash
docker save -o gpu-device-plugin.tar gpu-device-plugin:v1.0
docker save -o cuda-client-update.tar cuda-client-update:v1.0
```

## 3、安装部署

#### 1、前置准备：

##### 安装helm

参考 [https://helm.sh/zh/docs/v3/intro/install/](https://helm.sh/zh/docs/v3/intro/install/)，以下为节选步骤：

1. 下载需要的版本

2. 解压：

    ```Bash
    tar -zxvf helm-v3.0.0-linux-amd64.tar.gz
    ```

3. 在解压目录中找到helm程序，移动到需要的目录中：

    ```Bash
    mv linux-amd64/helm /usr/local/bin/helm
    ```

安装完成之后，执行如下命令将`client-update`（cuda劫持）、`gpu-device-plugin`（设备插件）安装部署yaml打包为helm chart包：

```Bash
cd Flex-AI-main/install/helm && helm package gpupool
```
获得 `gpupool-0.1.0.tgz`。

##### 安装nvidia-container-toolkit与nvidia driver

在运行业务的gpu节点上下载：
- nvidai driver [NVIDIA-Linux-x86_64-535.183.06.run](https://us.download.nvidia.cn/tesla/535.183.06/NVIDIA-Linux-x86_64-535.183.06.run)
- nvidia-container-toolkit [nvidia-container-toolkit_1.16.1_rpm_x86_64.tar.gz](https://github.com/NVIDIA/nvidia-container-toolkit/releases/download/v1.16.1/nvidia-container-toolkit_1.16.1_rpm_x86_64.tar.gz).

执行如下命令安装：

```Bash
# 安装nvidia driver
chmod +x NVIDIA-Linux-x86_64-*.run
./NVIDIA-Linux-x86_64-*.run \
    --silent \
    --accept-license \
    --no-questions \
    --disable-nouveau \
    --install-libglvnd \
    --no-x-check \
    --no-nouveau-check

# 安装nvidia-container-toolkit
tar -zxvf nvidia-container-toolkit_*.tar.gz
cd release-*-stable/packages/centos7/x86_64/
rpm -ivh libnvidia-container1-*.rpm \
    libnvidia-container-tools-*.rpm \
    nvidia-container-toolkit-*.rpm \
    nvidia-container-toolkit-base-*.rpm \
    nvidia-container-toolkit-operator-extensions-*.rpm
```

安装完成后可以通过查看`nvidia-smi`（驱动）、`nvidia-ctk`（nvidia-container-toolkit）命令的回显来确认是否安装成功。

##### 修改运行时

根据节点的运行时（docker/containerd），执行如下命令：
- docker：
```Bash
nvidia-ctk runtime configure --runtime=docker
systemctl daemon-reload
systemctl restart docker
```
- containerd：
```Bash
nvidia-ctk runtime configure --runtime=containerd
systemctl daemon-reload
systemctl restart containerd
```

##### 拉起虚拟组件：

将镜像和helm chart包上传至运行业务的gpu节点上，将镜像导入到节点上。执行如下命令：

```Bash
kubectl patch runtimeclass nvidia --type=merge \
    --patch '{"metadata":{"labels":{"app.kubernetes.io/managed-by":"helm"},"annotations":{"meta.helm.sh/release-name":"gpupool","meta.helm.sh/release-namespace":"default"}}}'
kubectl label node {node-name} gpupool.com/gpu-ready=true
helm install gpupool gpupool-0.1.0.tgz --set runtimeType="{runtimeType}" --set osType={osType}
```
其中：{runtimeType} 和 {osType} 应被替换为实际的值，示例如下：
helm install gpupool gpupool-0.1.0.tgz --set runtimeType="containerd" --set osType=ubuntu
- `runtimeType` 支持`docker`/`containerd`两种输入
- `osType` 支持`centos`、`rhel`、`ubuntu`、`debian`四种输入

安装完成后，使用`kubectl get pod -A`查看pod状态，`running`表示状态正常。



## 部署流程

1. 上传模型权重到希望调度的有xpu卡的工作节点的任意目录下

模型文件：`DeepSeek-R1-Distill-Llama-8B`

1. 拉取vllm镜像

```Bash
docker pull vllm/vllm-openai:latest
```

1. 登陆master节点，给有xpu卡的工作节点上标签

```Bash
kubectl label nodes <worker-node-name> node-type=llm
```

1. 在master节点创建命名空间

```Bash
kubectl create namespace deepseek-test
```

1. 在master节点编辑并上传`deployment.yaml`和`service.yaml`

- **deployment.yaml**

```YAML
apiVersion: apps/v1
kind: Deployment
metadata:
  name: deepseek-r1
  namespace: deepseek-test
  labels:
    app: deepseek-r1
spec:
  replicas: 1
  selector:
    matchLabels:
      app: deepseek-r1
  template:
    metadata:
      labels:
        app: deepseek-r1
    spec:
      # 使服务pod调度到标签节点上
      nodeSelector:
        node-type: llm
      volumes:
        # 挂载模型文件本地路径
        - name: local-models
          hostPath:
            path: {path/to/DeepSeek-R1-Distill-llama-8B-main}  # 填写模型权重路径
            type: Directory
      containers:
        - name: deepseek-r1
          image: vllm/vllm-openai:latest
          imagePullPolicy: IfNotPresent
          command: ["vllm", "serve"]
          args:
            - "{path/to/DeepSeek-R1-Distill-llama-8B-main}"  # 填写模型权重路径
            - "--trust-remote-code"
            - "--enable-chunked-prefill"
            - "--max-num-batched-tokens"
            - "4096"
            - "--load-format"
            - "safetensors"
            - "--gpu-memory-utilization"  # 用于模型执行器的GPU内存分配，范围0到1，默认以0.9
            - "0.9"
          ports:
            - containerPort: 8000
          resources:
            requests: # vgpu资源配置
              huawei.com/vgpu-number: 1
              huawei.com/vgpu-cores: 20
              huawei.com/vgpu-memory-1Gi: 3
          limits:
            huawei.com/vgpu-number: 1
            huawei.com/vgpu-cores: 20
            huawei.com/vgpu-memory-1Gi: 3
          volumeMounts:
            - name: local-models
              mountPath: {path/to/DeepSeek-R1-Distill-llama-8B-main}  # 填写模型权重路径
              readOnly: true
          # 存活探针，如果探针失败，容器会重启
          livenessProbe:
            httpGet:
              path: /health
              port: 8000
            initialDelaySeconds: 300  # 第一次执行等待时间，设置太大是会导致模型还没有起来，原因是因为挂载共享导致资源延后
            periodSeconds: 10
          # 就绪探针
          readinessProbe:
            httpGet:
              path: /health
              port: 8000
            initialDelaySeconds: 300
            periodSeconds: 5
```
在业务pod的yaml里申请vgpu资源, 其中：
|参数|说明|
|---|---|
|vgpu-number|表示申请的vgpu卡数|
|vgpu-cores|表示申请的算力切分百分比（0-100）%|
|vgpu-memory-1Gi|表示申请的显存占用(Gi)|

- {path/to/DeepSeek-R1-Distill-llama-8B-main} 应被替换为模型权重路径
	

体验算力切分效果，查看gpu卡是否在20%上下波动，执行
```BASH
kubectl exec -it core-usage-test-601 -- watch -n 1 /opt/xpu/bin/gpu-monitor -p 1`
```

- **service.yaml**
```YAML
apiVersion: v1
kind: Service
metadata:
  name: deepseek-r1
  namespace: deepseek-test
spec:
  ports:
    - name: http-deepseek-r1
      port: 80
      protocol: TCP
      targetPort: 8000
  selector:
    app: deepseek-r1
  sessionAffinity: None
  type: ClusterIP
```

## 启动

将镜像文件放入对应的本地挂载路径之后，执行：

```Bash
kubectl apply -f deployment.yaml
kubectl apply -f service.yaml
```

检查pod状态：

```Bash
kubectl get pods -A
kubectl describe pods <pod_name>
```

容器创建成功之后，查看执行日志：

```Bash
kubectl logs -n <namespace> <pod_name>
```

出现类似于：`INFO 03-27 23:19:15 api_server.py:958] Starting vLLM API server on http://0.0.0.0:8000`，即为成功。

查看GPU卡使用情况：

```Bash
watch -n 0.1 nvidia-smi
```

## 调用

首先获取IP地址：

```Bash
kubectl get pods -o wide -A
```

获取接口说明：

```Bash
curl -X GET "http://<service-ip>:8000/openapi.json"
```

聊天：

```Bash
curl -X POST http://<service-ip>:8000/v1/chat/completions -H "Content-Type: application/json" -d '{
  "model": "/home/gandalf/DeepSeek-R1-Distill-llama-8B-main",
  "messages": [
    {
      "role": "user",
      "content": "红楼梦是什么"
    }
  ]
}'
```

- 如果配置了`hostNetwork: true`也可以用postman请求。

## 补充：vllm参数说明

|参数|说明|默认值|
|---|---|---|
|load_format|模型权重加载的格式|"auto"|
|gpu-memory-utilization|用于模型执行器的GPU内存分配，范围0到1|0.9|
|max-model-len|模型上下文长度，如果未指定，将自动从模型配置中推导|config.json#max_position_embeddings|
|tensor-parallel-size|张量并行的副本数量|-|

- vLLM默认通过gpu-memory-utilization参数（默认值0.9）预分配GPU显存以支持动态KV缓存

- `gpu-memory-utilization`参数下调后，如果出现启动失败，需要根据错误信息对应下调`--max-model-len`参数



