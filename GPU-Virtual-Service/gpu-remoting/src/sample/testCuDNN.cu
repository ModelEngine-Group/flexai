/**
* Initialized CUDA and cuDNN context.
* Input data initialized: 
* 0 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 
* Input Data prepared: tensors allocated and initialized.
* Extra parameters and workspace for batch normalization allocated.
* Forward and backward propagation completed.
* Batch Normalization Output:
* -1.62621 -1.62619 -1.62617 -1.62615 -1.62613 -1.62611 -1.6261 -1.62608 -1.62606 -1.62604 
* Input Gradient:
* 4.17587e-42 4.17587e-42 4.17587e-42 4.17587e-42 4.17587e-42 4.17587e-42 4.17587e-42 4.17587e-42 4.17587e-42 4.17587e-42
*/
#include <cudnn.h>
#include <cuda_runtime.h>
#include <iostream>
#include <vector>

#define checkCUDNN(expression)                               \
{                                                            \
    cudnnStatus_t status = (expression);                     \
    if (status != CUDNN_STATUS_SUCCESS) {                    \
        std::cerr << "Error on line " << __LINE__ << ": "    \
                  << cudnnGetErrorString(status) << std::endl; \
        std::exit(EXIT_FAILURE);                             \
    }                                                        \
}

#define checkCUDA(expression)                                \
{                                                            \
    cudaError_t error = (expression);                        \
    if (error != cudaSuccess) {                              \
        std::cerr << "Error on line " << __LINE__ << ": "    \
                  << cudaGetErrorString(error) << std::endl; \
        std::exit(EXIT_FAILURE);                             \
    }                                                        \
}

// CUDA内核，用于初始化输入数据为等差数列
__global__ void initArithmeticSequence(float *data, int n, float start, float step) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        data[idx] = start + step * idx;
    }
}

int main() {
    cudnnHandle_t cudnn;
    checkCUDNN(cudnnCreate(&cudnn)); // 初始化cuDNN

    cudaStream_t stream;
    checkCUDA(cudaStreamCreate(&stream)); // 初始化CUDA流
    checkCUDNN(cudnnSetStream(cudnn, stream)); // 设置cuDNN流

    // 数据维度和批量标准化参数
    const int batch_size = 64; // 批大小
    const int channels = 3;    // 输入通道数
    const int height = 32;     // 图像高度
    const int width = 32;      // 图像宽度
    const float epsilon = 1e-5f; // 用于批量标准化的epsilon值
    const float momentum = 0.9f; // 用于移动平均的动量值

    std::cout << "Initialized CUDA and cuDNN context." << std::endl;

    // 设定输入输出张量的维度
    const int tensorDims = 4;
    int tensorDimsA[tensorDims] = {batch_size, channels, height, width}; // 定义维度大小
    int strideA[tensorDims] = {channels * height * width, height * width, width, 1}; // 定义步长

    // 创建张量描述符
    cudnnTensorDescriptor_t inputDescriptor, outputDescriptor, dyDescriptor, dxDescriptor;
    checkCUDNN(cudnnCreateTensorDescriptor(&inputDescriptor));
    checkCUDNN(cudnnCreateTensorDescriptor(&outputDescriptor));
    checkCUDNN(cudnnCreateTensorDescriptor(&dyDescriptor)); // 输出梯度描述符
    checkCUDNN(cudnnCreateTensorDescriptor(&dxDescriptor)); // 输入梯度描述符

    // 设置张量描述符的格式和维度
    checkCUDNN(cudnnSetTensorNdDescriptor(inputDescriptor,
                                        CUDNN_DATA_FLOAT,
                                        tensorDims,
                                        tensorDimsA,
                                        strideA));
    // checkCUDNN(cudnnSetTensor4dDescriptor(outputDescriptor,
    //                                     CUDNN_TENSOR_NCHW,
    //                                     CUDNN_DATA_FLOAT,
    //                                     batch_size,
    //                                     channels,
    //                                     height,
    //                                     width));
    checkCUDNN(cudnnSetTensorNdDescriptorEx(outputDescriptor,
                                        CUDNN_TENSOR_NCHW,
                                        CUDNN_DATA_FLOAT,
                                        tensorDims, 
                                        tensorDimsA));
    checkCUDNN(cudnnSetTensor4dDescriptor(dyDescriptor,
                                        CUDNN_TENSOR_NCHW,
                                        CUDNN_DATA_FLOAT,
                                        batch_size,
                                        channels,
                                        height,
                                        width));
    checkCUDNN(cudnnSetTensor4dDescriptor(dxDescriptor,
                                        CUDNN_TENSOR_NCHW,
                                        CUDNN_DATA_FLOAT,
                                        batch_size,
                                        channels,
                                        height,
                                        width));

    // 为输入数据、输出数据、批量标准化参数分配GPU内存
    float *d_input, *d_output, *d_output_grad, *d_input_grad;
    float *d_scale, *d_bias;

    checkCUDA(cudaMalloc(&d_input, sizeof(float) * batch_size * channels * height * width));
    checkCUDA(cudaMalloc(&d_output, sizeof(float) * batch_size * channels * height * width));
    checkCUDA(cudaMalloc(&d_output_grad, sizeof(float) * batch_size * channels * height * width)); // 输出梯度
    checkCUDA(cudaMalloc(&d_input_grad, sizeof(float) * batch_size * channels * height * width)); // 输入梯度
    checkCUDA(cudaMalloc(&d_scale, sizeof(float) * channels));
    checkCUDA(cudaMalloc(&d_bias, sizeof(float) * channels));

    // 初始化输入数据为等差数列
    int n = batch_size * channels * height * width;
    float start = 0.0f; // 等差数列起始值
    float step = 0.1f;  // 等差数列步长
    int threadsPerBlock = 256;
    int blocksPerGrid = (n + threadsPerBlock - 1) / threadsPerBlock;
    initArithmeticSequence<<<blocksPerGrid, threadsPerBlock>>>(d_input, n, start, step);
    cudaDeviceSynchronize(); // 确保CUDA内核执行完成
    float *h_input = new float[n];
    checkCUDA(cudaMemcpy(h_input, d_input, sizeof(float) * n, cudaMemcpyDeviceToHost));
    std::cout << "Input data initialized: " << std::endl;
    for (int i = 0; i < 10; i++) {
        std::cout << h_input[i] << " ";
    }
    std::cout << std::endl;

    // 初始化批量标准化参数
    std::vector<float> h_scale(channels, 1.0f); // scale初始化为1
    std::vector<float> h_bias(channels, 0.0f);  // bias初始化为0

    // 将初始化的参数从主机拷贝到设备
    checkCUDA(cudaMemcpy(d_scale, h_scale.data(), sizeof(float) * channels, cudaMemcpyHostToDevice));
    checkCUDA(cudaMemcpy(d_bias, h_bias.data(), sizeof(float) * channels, cudaMemcpyHostToDevice));

    std::cout << "Input Data prepared: tensors allocated and initialized." << std::endl;

    // 前向传播和反向传播的额外参数
    float *d_runningMean, *d_runningVariance;
    float *d_saveMean, *d_saveInvVariance; // 前向训练时保存的均值和逆方差
    float *d_bnScaleDiff, *d_bnBiasDiff; // 反向传播时计算的梯度
    void *d_workspace = nullptr; // 反向传播所需的工作空间
    size_t workspaceSize = 0; // 工作空间的大小
    size_t reserveSpaceSize = 0; // 保留空间的大小

    // 分配额外参数的内存
    checkCUDA(cudaMalloc(&d_runningMean, sizeof(float) * channels));
    checkCUDA(cudaMalloc(&d_runningVariance, sizeof(float) * channels));
    checkCUDA(cudaMalloc(&d_saveMean, sizeof(float) * channels));
    checkCUDA(cudaMalloc(&d_saveInvVariance, sizeof(float) * channels));
    checkCUDA(cudaMalloc(&d_bnScaleDiff, sizeof(float) * channels));
    checkCUDA(cudaMalloc(&d_bnBiasDiff, sizeof(float) * channels));

    // 初始化为0
    checkCUDA(cudaMemset(d_runningMean, 0, sizeof(float) * channels));
    checkCUDA(cudaMemset(d_runningVariance, 0, sizeof(float) * channels));

    // 获取前向传播和反向传播所需的工作空间大小
    cudnnTensorDescriptor_t bnScaleBiasMeanVarDesc;
    checkCUDNN(cudnnCreateTensorDescriptor(&bnScaleBiasMeanVarDesc));
    checkCUDNN(cudnnSetTensor4dDescriptor(bnScaleBiasMeanVarDesc,
                                        CUDNN_TENSOR_NCHW,
                                        CUDNN_DATA_FLOAT,
                                        1, channels, 1, 1));

    cudnnBatchNormMode_t mode = CUDNN_BATCHNORM_SPATIAL_PERSISTENT;
    cudnnBatchNormOps_t bnOps = CUDNN_BATCHNORM_OPS_BN;
    // 假设与批量标准化操作不结合使用任何额外的激活函数
    cudnnActivationDescriptor_t activationDesc = nullptr;

    // 为前向训练计算工作空间大小
    checkCUDNN(cudnnGetBatchNormalizationForwardTrainingExWorkspaceSize(
        cudnn, // cudnn句柄
        mode,  // 批量标准化模式
        bnOps, // 批量标准化操作
        inputDescriptor, // xDesc: 输入张量描述符
        nullptr, // zDesc: 偏置张量描述符（在这里不使用，因此为nullptr）
        outputDescriptor, // yDesc: 输出张量描述符
        bnScaleBiasMeanVarDesc, // bnScaleBiasMeanVarDesc: 缩放、偏置、均值和方差的描述符
        activationDesc, // activationDesc: 激活描述符（在这里不使用，因此为nullptr）
        &workspaceSize // sizeInBytes: 工作空间大小的指针
    ));

    // 为训练计算保留空间大小
    checkCUDNN(cudnnGetBatchNormalizationTrainingExReserveSpaceSize(
        cudnn, // cudnn句柄
        mode,  // 批量标准化模式
        bnOps, // 批量标准化操作
        activationDesc, // activationDesc: 激活描述符（在这里不使用，因此为nullptr）
        inputDescriptor, // xDesc: 输入张量描述符
        &reserveSpaceSize // sizeInBytes: 保留空间大小的指针
    ));

    // 分配工作空间和保留空间
    void *d_reserveSpace = nullptr; // 用于反向传播的保留空间
    if (workspaceSize > 0) {
        checkCUDA(cudaMalloc(&d_workspace, workspaceSize));
        std::cout << "Workspace for batch normalization allocated: " << workspaceSize << " bytes." << std::endl;
    }
    if (reserveSpaceSize > 0) {
        checkCUDA(cudaMalloc(&d_reserveSpace, reserveSpaceSize));
        std::cout << "Reserve space for batch normalization allocated: " << reserveSpaceSize << " bytes." << std::endl;
    }

    std::cout << "Extra parameters and workspace for batch normalization allocated." << std::endl;

    // 设置学习率参数，这些在实际训练中可能会变化或根据优化算法调整
    const float alpha = 1.0f;
    const float beta = 0.0f;

    // 前向训练
    checkCUDNN(cudnnBatchNormalizationForwardTrainingEx(
        cudnn, mode, bnOps, 
        &alpha, &beta, inputDescriptor, d_input, nullptr, nullptr, 
        outputDescriptor, d_output, bnScaleBiasMeanVarDesc,
        d_scale, d_bias, momentum, d_runningMean, d_runningVariance, 
        epsilon, d_saveMean, d_saveInvVariance, activationDesc, 
        d_workspace, workspaceSize, d_reserveSpace, reserveSpaceSize
    ));

    // 前向推理（用于评估或推理）
    // 使用训练过程中计算得到的运行均值和方差
    checkCUDNN(cudnnBatchNormalizationForwardInference(
        cudnn, mode,
        &alpha, &beta, inputDescriptor, d_input,
        outputDescriptor, d_output, bnScaleBiasMeanVarDesc,
        d_scale, d_bias, d_runningMean, d_runningVariance, epsilon
    ));

    // 执行批量标准化的反向传播
    const float alphaDataDiff = 1.0f; // 输出梯度缩放系数
    const float betaDataDiff = 0.0f;
    const float alphaParamDiff = 1.0f; // 参数梯度缩放系数
    const float betaParamDiff = 0.0f;
    const float outputGradVal = 1.0f;
    checkCUDA(cudaMemset(d_output_grad, outputGradVal, sizeof(float) * batch_size * channels * height * width)); // 输出梯度初始化为1

    size_t workspaceSizeBackward = 0; // 反向传播所需的工作空间大小 
    checkCUDNN(cudnnGetBatchNormalizationBackwardExWorkspaceSize(
        cudnn, // cudnn句柄
        mode,  // 批量标准化模式
        bnOps, // 批量标准化操作
        inputDescriptor, // xDesc: 输入张量描述符
        outputDescriptor, // yDesc: 输出张量描述符
        dyDescriptor, // dyDesc: 输出梯度张量描述符
        nullptr, // dzDesc: 输入梯度张量描述符（在这里不使用，因此为nullptr）
        dxDescriptor, // dxDesc: 输入梯度张量描述符
        bnScaleBiasMeanVarDesc, // bnScaleBiasMeanVarDesc: 缩放、偏置、均值和方差的描述符
        activationDesc, // activationDesc: 激活描述符（如果使用激活函数的话）
        &workspaceSizeBackward // sizeInBytes: 指向存储工作空间大小的变量的指针
    ));
    if (workspaceSizeBackward > 0) {
        if (workspaceSizeBackward > workspaceSize) {
            // 如果反向传播需要的工作空间比前向传播更大，重新分配
            cudaFree(d_workspace); // 释放之前分配的工作空间
            checkCUDA(cudaMalloc(&d_workspace, workspaceSizeBackward));
        }
        // 如果前向传播的工作空间已经足够大，则无需重新分配
        std::cout << "Workspace for batch normalization backward allocated: " << workspaceSizeBackward << " bytes." << std::endl;
    }

    checkCUDNN(cudnnBatchNormalizationBackwardEx(
        cudnn, mode, bnOps, 
        &alphaDataDiff, &betaDataDiff, &alphaParamDiff, &betaParamDiff,
        inputDescriptor, d_input, outputDescriptor, d_output,
        dyDescriptor, d_output_grad, nullptr, nullptr, dxDescriptor, d_input_grad,
        bnScaleBiasMeanVarDesc, d_scale, d_bias, d_bnScaleDiff, d_bnBiasDiff,
        epsilon, d_saveMean, d_saveInvVariance, activationDesc,
        d_workspace, workspaceSize, d_reserveSpace, reserveSpaceSize
    ));

    std::cout << "Forward and backward propagation completed." << std::endl;

    // 将输出数据从GPU拷贝回主机
    float *h_output;
    h_output = new float[batch_size * channels * height * width];
    checkCUDA(cudaMemcpy(h_output, d_output, sizeof(float) * batch_size * channels * height * width, cudaMemcpyDeviceToHost));

    // 将输入梯度从GPU拷贝回主机
    float *h_input_grad;
    h_input_grad = new float[batch_size * channels * height * width];
    checkCUDA(cudaMemcpy(h_input_grad, d_input_grad, sizeof(float) * batch_size * channels * height * width, cudaMemcpyDeviceToHost));

    // 输出批量标准化的结果
    std::cout << "Batch Normalization Output:" << std::endl;
    for (int i = 0; i < 10; i++) {
        std::cout << h_output[i] << " ";
    }
    std::cout << std::endl;
    // -1.62621 -1.62619 -1.62617 -1.62615 -1.62613 -1.62611 -1.6261 -1.62608 -1.62606 -1.6260

    // 输出输入梯度
    std::cout << "Input Gradient:" << std::endl;
    for (int i = 0; i < 10; i++) {
        std::cout << h_input_grad[i] << " ";
    }
    std::cout << std::endl;
    // 4.17587e-42 4.17587e-42 4.17587e-42 4.17587e-42 4.17587e-42 4.17587e-42 4.17587e-42 4.17587e-42 4.17587e-42 4.17587e-42

    // 释放主机内存
    delete[] h_output;
    delete[] h_input_grad;

    // 同步GPU并释放内存
    cudaDeviceSynchronize();
    cudaFree(d_input);
    cudaFree(d_output);
    cudaFree(d_output_grad);
    cudaFree(d_input_grad);
    cudaFree(d_scale);
    cudaFree(d_bias);
    cudaFree(d_runningMean);
    cudaFree(d_runningVariance);
    cudaFree(d_saveMean);
    cudaFree(d_saveInvVariance);
    cudaFree(d_bnScaleDiff);
    cudaFree(d_bnBiasDiff);
    cudaFree(d_workspace);
    cudaFree(d_reserveSpace);

    // 清理cuDNN资源
    cudnnDestroyTensorDescriptor(inputDescriptor);
    cudnnDestroyTensorDescriptor(outputDescriptor);
    cudnnDestroyTensorDescriptor(dyDescriptor);
    cudnnDestroyTensorDescriptor(dxDescriptor);
    cudnnDestroyTensorDescriptor(bnScaleBiasMeanVarDesc);
    cudnnDestroy(cudnn);
    cudaStreamDestroy(stream);

    return 0;
}