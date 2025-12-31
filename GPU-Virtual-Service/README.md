## Flex:ai 本地XPU虚拟化

Flex:ai开源项目，提供将XPU算力卡进行虚拟化切分，以及面向AI训推任务和集群资源做智能调度的能力。

## 安装说明

### 1、源码编译

#### 1.1 cuda劫持

1. 安装cuda：

```bash
wget https://developer.download.nvidia.com/compute/cuda/12.2.0/local_installers/cuda_12.2.0_535.54.03_linux.run
sudo sh cuda_12.2.0_535.54.03_linux.run
```

2. 安装依赖项

下载 [https://github.com/gabime/spdlog/archive/refs/tags/v1.12.0.zip](https://github.com/gabime/spdlog/archive/refs/tags/v1.12.0.zip) ，执行：

```Bash
mkdir -p /usr/local/include
unzip spdlog*.zip && cd spdlog-1.12.0 && cp -rf include/spdlog /usr/local/include
```

下载 [https://github.com/NixOS/patchelf/archive/refs/tags/0.18.0.zip](https://github.com/NixOS/patchelf/archive/refs/tags/0.18.0.zip) ，执行：

```Bash
unzip patchelf-0.18.0.zip && cd patchelf-0.18.0
yum install autoconf automake libtool
./bootstrap.sh
./configure
make
sudo make install
```

3. 编译

下载`GPU-Virtual-Service`代码文件，在`GPU-Virtual-Service/xpu-pool-service/`路径中 ，执行：

```Bash
cd direct && chmod +x make_lib_original.sh && cd -
mkdir build && cd build && cmake .. && make -j
```

编译后生成的动态库文件在`direct/cuda`目录下，为`libcuda_direct.so`、`gpu-monitor`。

#### 1.2 设备插件

go的版本为1.25.0，建议保持一致：

```Bash
export CGO_ENABLED=1
cd Flex-AI-main/GPU-device-plugin && go mod tidy && make
```
编译生成文件：`gpu-device-plugin`、`xpu-client-tool`。

#### 1.3 调度组件

调度组件的编译后文件可以在`lib/`文件夹中找到。

`huawei-xpu.so`、`vc-scheduler`、`vc-controller-manager`、`vc-webhook-manage`。

### 2、打包镜像

将编译后产出的文件复制到`docker-build`对应的目录下，在`GPU-Virtual-Service/`目录下执行：

```Bash
cp -rf xpu-pool-service/build/direct/cuda/libcuda_direct.so docker-build/client-update
cp -rf xpu-pool-service/build/direct/cuda/gpu-monitor docker-build/client-update
chmod +x xpu-pool-service/client_update/cuda-client-update.sh && cp -rf xpu-pool-service/client_update/cuda-client-update.sh docker-build/client-update
cp -rf xpu-pool-service/GPU-device-plugin/xpu-client-tool docker-build/client-update
```

其中除`cuda-client-update.sh`为脚本文件，剩下的均为编译结果

```Bash
cp -rf xpu-pool-service/GPU-device-plugin/gpu-device-plugin docker-build/gpu-device-plugin
```

在`docker-build/client-update`目录下执行：

```Bash
docker build -t cuda-client-update:2.0 .
```

在`docker-build/gpu-device-plugin`目录下执行：

```Bash
docker build -t gpu-device-plugin:2.0 .
```

（上述代码的`.`不能忽略）

至此，镜像制作完成，可以使用如下命令将镜像保存到本地：

```Bash
docker save -o gpu-device-plugin.tar gpu-device-plugin:2.0
docker save -o cuda-client-update.tar cuda-client-update:2.0
```

volcano调度的打包流程与上述相仿，将编译产物复制到对应的`docker-build/`所在的文件夹下：
`controller-manage`：`vc-controller-manager`
`scheduler`：`vc-scheduler`、`huawei-xpu.so`
`webhook-manage`：`vc-webhook-manager`

然后在`docker-build/`所在的目录下执行`docker build -t {image_name}:v1.10.2 .`
`{image_name}`分别为`vc-scheduler`、`vc-controller`、`vc-webhook-manage`，与对应的文件夹名称保持一致。

### 3、安装部署

#### 3.1 前置准备

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

#### 3.2 拉起虚拟组件

将镜像和helm chart包上传至运行业务的gpu节点上，将镜像导入到节点上。执行如下命令：

```Bash
kubectl patch runtimeclass nvidia --type=merge \
    --patch '{"metadata":{"labels":{"app.kubernetes.io/managed-by":"helm"},"annotations":{"meta.helm.sh/release-name":"gpupool","meta.helm.sh/release-namespace":"default"}}}'
kubectl label node {node-name} gpupool.com/gpu-ready=true
helm install gpupool gpupool-0.1.0.tgz --set runtimeType="{runtimeType}" --set osType={osType}
```

其中：{runtimeType} 和 {osType} 应被替换为实际的值，示例如下：
helm install gpupool gpupool-0.1.0.tgz --set runtimeType="containerd" --set osType=centos

- `runtimeType` 支持`docker`/`containerd`两种输入
- `osType` 支持`centos`、`rhel`、`ubuntu`、`debian`四种输入。**（当前仅支持`centos`）**

安装完成后，使用`kubectl get pod -A`查看pod状态，`running`表示状态正常。

## 本地XPU虚拟化应用

**版本约束：**

- 支持的操作系统类型：openEuler（cgroup v1）[推荐下载链接](https://www.openeuler.org/en/download/archive/detail/?version=openEuler%2022.03%20LTS%20SP2)

- K8s支持版本：1.31.1

### 部署流程

1. 上传模型权重到希望调度的有xpu卡的工作节点任意目录下

模型文件：`DeepSeek-R1-Distill-Llama-8B`

2. 拉取vllm镜像

```Bash
docker pull vllm/vllm-openai:latest
```

3. 登陆master节点，给有xpu卡的工作节点上标签

```Bash
kubectl label nodes <worker-node-name> node-type=llm
```

4. 在master节点创建命名空间

```Bash
kubectl create namespace deepseek-test
```

5. 在master节点编辑并上传`deployment.yaml`和`service.yaml`

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
            initialDelaySeconds: 300  # 第一次执行等待时间，设置太大会导致模型还没有起来，原因是挂载共享导致资源延后
            periodSeconds: 10
          # 就绪探针
          readinessProbe:
            httpGet:
              path: /health
              port: 8000
            initialDelaySeconds: 300
            periodSeconds: 5
```

6. 在业务pod的yaml里申请vgpu资源, 其中：

| 参数            | 说明                               |
| --------------- | ---------------------------------- |
| vgpu-number     | 表示申请的vgpu卡数                 |
| vgpu-cores      | 表示申请的算力切分百分比（0-100）% （只支持5的倍数）|
| vgpu-memory-1Gi | 表示申请的显存占用(Gi)             |

- {path/to/DeepSeek-R1-Distill-llama-8B-main} 应被替换为模型权重路径

- 规格：

  一张GPU卡可以分1-5个vgpu，可以给1-5个容器使用。

  一个容器能够使用的vgpu数量小于等于当前节点物理GPU卡数量。

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

### 启动

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

### 调用

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

### 体验算力切分效果

查看gpu卡最高利用率在是否在设定的`vgpu-cores`百分比上下波动，执行
```BASH
kubectl exec -it <pod_name> -- watch -n 1 /opt/xpu/bin/gpu-monitor -p 1
```

## 补充说明

### vllm参数说明

| 参数                   | 说明                                               | 默认值                              |
| ---------------------- | -------------------------------------------------- | ----------------------------------- |
| load_format            | 模型权重加载的格式                                 | "auto"                              |
| gpu-memory-utilization | 用于模型执行器的GPU内存分配，范围0到1              | 0.9                                 |
| max-model-len          | 模型上下文长度，如果未指定，将自动从模型配置中推导 | config.json中max_position_embeddings |
| tensor-parallel-size   | 张量并行的副本数量                                 | -                                   |

- vLLM默认通过gpu-memory-utilization参数（默认值0.9）预分配GPU显存以支持动态KV缓存

- `gpu-memory-utilization`参数下调后，如果出现启动失败，需要根据错误信息对应下调`--max-model-len`参数



