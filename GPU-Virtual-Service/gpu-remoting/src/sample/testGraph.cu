#include <iostream>
#include <cuda_runtime.h>

#define N 1024 * 1024  // 数据大小
#define THREADS_PER_BLOCK 256

__global__ void addKernel(float* data, int n) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        data[idx] = data[idx] + 1;
    }
}

// 检查 CUDA 错误
void checkCudaError(cudaError_t err, const char* msg) {
    if (err != cudaSuccess) {
        std::cerr << msg << ": " << cudaGetErrorString(err) << std::endl;
        exit(EXIT_FAILURE);
    }
}

int main() {
    // 1. 初始化数据
    float* h_data = new float[N];
    for (int i = 0; i < N; ++i) {
        h_data[i] = static_cast<float>(i);
    }

    float* d_data;
    checkCudaError(cudaMalloc(&d_data, N * sizeof(float)), "Failed to allocate device memory");
    checkCudaError(cudaMemcpy(d_data, h_data, N * sizeof(float), cudaMemcpyHostToDevice), "Failed to copy data to device");

    // 2. 创建 CUDA Stream
    cudaStream_t stream;
    checkCudaError(cudaStreamCreate(&stream), "Failed to create CUDA stream");

    // 3. CUDA Graph 相关变量
    cudaGraph_t graph;
    cudaGraphExec_t graphExec;

    // 4. 开始捕获 CUDA Graph
    checkCudaError(cudaStreamBeginCapture(stream, cudaStreamCaptureModeGlobal), "Failed to begin CUDA Graph capture");

    // 计算内核配置
    int blocks = (N + THREADS_PER_BLOCK - 1) / THREADS_PER_BLOCK;
    addKernel<<<blocks, THREADS_PER_BLOCK, 0, stream>>>(d_data, N);

    // 停止捕获并生成 CUDA Graph
    checkCudaError(cudaStreamEndCapture(stream, &graph), "Failed to end CUDA Graph capture");

    // 5. 实例化 CUDA Graph
    checkCudaError(cudaGraphInstantiate(&graphExec, graph, nullptr, nullptr, 0), "Failed to instantiate CUDA Graph");

    // 6. 重复执行 CUDA Graph
    const int iterations = 5;
    for (int i = 0; i < iterations; ++i) {
        checkCudaError(cudaGraphLaunch(graphExec, stream), "Failed to launch CUDA Graph");
        checkCudaError(cudaStreamSynchronize(stream), "Failed to synchronize stream");
    }

    cudaDeviceSynchronize();

    // 7. 将结果从设备复制回主机
    checkCudaError(cudaMemcpy(h_data, d_data, N * sizeof(float), cudaMemcpyDeviceToHost), "Failed to copy data back to host");

    // 8. 检查结果
    for (int i = 0; i < 5; ++i) {
        std::cout << "h_data[" << i << "] = " << h_data[i] << std::endl;
    }

    // 9. 释放资源
    cudaGraphExecDestroy(graphExec);
    cudaGraphDestroy(graph);
    cudaStreamDestroy(stream);
    cudaFree(d_data);
    delete[] h_data;

    return 0;
}