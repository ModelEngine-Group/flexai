#include <iostream>
#include <cuda.h>
#include <cuda_runtime.h>

// 错误检查宏
#define CHECK_CUDA(call) { \
    cudaError_t cudaStatus = call; \
    if (cudaStatus != cudaSuccess) { \
        fprintf(stderr, "CUDA Error: %s\n", cudaGetErrorString(cudaStatus)); \
        exit(-1); \
    } \
}

// 核函数
__global__ void addOne(float *data, int size) {
    int index = blockIdx.x * blockDim.x + threadIdx.x;
    if (index < size) {
        data[index] += 1.0f;
    }
}

int main() {
    const int size = 9;
    float *h_data = new float[size]; // 主机端内存
    float *d_data;                   // 设备端内存
    const int bytes = size * sizeof(float);

    // 初始化数据
    for (int i = 0; i < size; ++i) {
        h_data[i] = 1.11111111111f * (i + 1);
    }

    // 在 GPU 上分配内存
    CHECK_CUDA(cudaMalloc((void **)&d_data, bytes));

    // 异步拷贝数据到 GPU
    CHECK_CUDA(cudaMemcpyAsync(d_data, h_data, bytes, cudaMemcpyHostToDevice));

    // 确定线程块大小和网格大小
    int blockSize = 256;
    int gridSize = (size + blockSize - 1) / blockSize;

    struct cudaFuncAttributes funcAttrib;
    CHECK_CUDA(cudaFuncGetAttributes(&funcAttrib, addOne));
    std::cout << "Kernel Function Info:" << "\n"
              << "    Max Threads Per Block: " << funcAttrib.maxThreadsPerBlock << "\n"
              << "    Shared Memory Per Block: " << funcAttrib.sharedSizeBytes << "\n"
              << "    Constant Memory Per Block: " << funcAttrib.constSizeBytes << "\n"
              << "    Local Memory Per Block: " << funcAttrib.localSizeBytes << "\n"
              << "    Registers Per Block: " << funcAttrib.numRegs << "\n"
              << "    PTX Version: " << funcAttrib.ptxVersion << "\n"
              << "    Binary Version: " << funcAttrib.binaryVersion << std::endl;

    // 启动核函数
    addOne<<<gridSize, blockSize>>>(d_data, size);
    CHECK_CUDA(cudaPeekAtLastError()); // 检查核函数是否启动成功

    // 异步拷贝结果回主机
    CHECK_CUDA(cudaMemcpyAsync(h_data, d_data, bytes, cudaMemcpyDeviceToHost));

    // 等待 GPU 完成
    CHECK_CUDA(cudaDeviceSynchronize());

    // 打印结果
    std::cout << "Results:" << std::endl;
    for (int i = 0; i < size; ++i) {
        std::cout << h_data[i] << std::endl;
    }

    // 清理
    delete[] h_data;
    cudaFree(d_data);

    // // 假设有一个CUDA操作，这里直接使用一个错误码模拟
    // CUresult result = CUDA_ERROR_INVALID_VALUE; // 示例错误码

    // // 获取错误描述字符串
    // const char* errorStr;
    // CUresult status = cuGetErrorString(result, &errorStr);

    // if (status == CUDA_SUCCESS) {
    //     std::cout << "CUDA Error: " << errorStr << std::endl;
    // } else {
    //     std::cout << "Failed to get CUDA error string." << std::endl;
    // }

    return 0;
}
