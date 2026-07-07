/**
* cuBLASLt library version:111003
* Heuristic result:
* algorithm descriptor:11,4294967296,0,476754254757889,0,0,68,80,
* Actual size of workspace memory required:0
* Result status:Success
* Result matrix:
* 90 114 138 
* 54 69 84 
* 18 24 30
*/
#include <iostream>
#include <vector>
#include <cublasLt.h>
#include <cuda_runtime.h>

// CUDA 和 cuBLASLt 错误检查宏
#define CHECK_CUDA(expression)                               \
{                                                            \
    cudaError_t error = (expression);                        \
    if (error != cudaSuccess) {                              \
        std::cerr << "Error on line " << __LINE__ << ": "    \
                  << cudaGetErrorString(error) << std::endl; \
        std::exit(EXIT_FAILURE);                             \
    }                                                        \
}

#define CHECK_CUBLASLT(expression)                               \
{                                                            \
    cublasStatus_t cublasLtStatus = (expression);                     \
    if (cublasLtStatus != CUBLAS_STATUS_SUCCESS) {                    \
        std::cerr << "Error on line " << __LINE__ << ": "    \
                  << cublasLtGetStatusString(cublasLtStatus) << std::endl; \
        std::exit(EXIT_FAILURE);                             \
    }                                                        \
}

int main() {
    const uint64_t N = 3;
    float a[N*N] = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0};
    float b[N*N] = {9.0, 8.0, 7.0, 6.0, 5.0, 4.0, 3.0, 2.0, 1.0};
    float c[N*N] = {0};

    std::cout << "cuBLASLt library version:" << cublasLtGetVersion() << std::endl;

    // 创建 cuBLASLt 句柄
    cublasLtHandle_t handle;
    CHECK_CUBLASLT(cublasLtCreate(&handle));

    // 在 GPU 上分配内存
    float *d_a, *d_b, *d_c;
    CHECK_CUDA(cudaMalloc((void **)&d_a, sizeof(a)));
    CHECK_CUDA(cudaMalloc((void **)&d_b, sizeof(b)));
    CHECK_CUDA(cudaMalloc((void **)&d_c, sizeof(c)));

    // 将数据复制到设备
    CHECK_CUDA(cudaMemcpy(d_a, a, sizeof(a), cudaMemcpyHostToDevice));
    CHECK_CUDA(cudaMemcpy(d_b, b, sizeof(b), cudaMemcpyHostToDevice));

    // 定义矩阵乘法操作的描述
    cublasLtMatmulDesc_t operationDesc;
    cublasLtMatrixLayout_t aLayout, bLayout, cLayout;
    cublasLtMatmulPreference_t preference;
    int returnedResults = 0;
    cublasLtMatmulHeuristicResult_t heuristicResult;

    CHECK_CUBLASLT(cublasLtMatmulDescCreate(&operationDesc, CUBLAS_COMPUTE_32F, CUDA_R_32F));
    cublasOperation_t transa = CUBLAS_OP_N; // 设置矩阵 A 的转置类型
    CHECK_CUBLASLT(cublasLtMatmulDescSetAttribute(operationDesc, CUBLASLT_MATMUL_DESC_TRANSA, &transa, sizeof(transa)));

    CHECK_CUBLASLT(cublasLtMatrixLayoutCreate(&aLayout, CUDA_R_32F, N, N - 2, N));
    CHECK_CUBLASLT(cublasLtMatrixLayoutCreate(&bLayout, CUDA_R_32F, N, N, N));
    CHECK_CUBLASLT(cublasLtMatrixLayoutCreate(&cLayout, CUDA_R_32F, N, N, N));
    CHECK_CUBLASLT(cublasLtMatrixLayoutSetAttribute(aLayout, CUBLASLT_MATRIX_LAYOUT_COLS, &N, sizeof(N)));

    size_t workspaceSize = 10 * 1024 * 1024;
    CHECK_CUBLASLT(cublasLtMatmulPreferenceCreate(&preference));
    CHECK_CUBLASLT(cublasLtMatmulPreferenceSetAttribute(preference, CUBLASLT_MATMUL_PREF_MAX_WORKSPACE_BYTES, &workspaceSize, sizeof(workspaceSize)));

    // 获取最佳算法
    CHECK_CUBLASLT(cublasLtMatmulAlgoGetHeuristic(handle, operationDesc, aLayout, bLayout, cLayout, cLayout, preference, 1, &heuristicResult, &returnedResults));

    if (returnedResults == 0) {
        std::cerr << "No suitable algorithm found!" << std::endl;
        exit(-1);
    }
    else {
        std::cout << "Heuristic result:" << std::endl;
        std::cout << "algorithm descriptor:";
        for (int i = 0; i < 8; i ++) {
            std::cout << heuristicResult.algo.data[i] << ",";
        }
        std::cout << std::endl << "Actual size of workspace memory required:" << heuristicResult.workspaceSize << std::endl;
        std::cout << "Result status:" << (heuristicResult.state == CUBLAS_STATUS_SUCCESS ? "Success" : "Failed") << std::endl;
    }

    // 执行矩阵乘法
    const float alpha = 1.0f, beta = 0.0f;
    CHECK_CUBLASLT(cublasLtMatmul(handle, operationDesc, &alpha, d_a, aLayout, d_b, bLayout, &beta, d_c, cLayout, d_c, cLayout, &heuristicResult.algo, nullptr, 0, 0));

    // 将结果复制回主机
    CHECK_CUDA(cudaMemcpy(c, d_c, sizeof(c), cudaMemcpyDeviceToHost));

    // 打印结果
    std::cout << "Result matrix:" << std::endl;
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            std::cout << c[i * N + j] << " ";
        }
        std::cout << std::endl;
    }

    // 清理资源
    cublasLtMatrixLayoutDestroy(aLayout);
    cublasLtMatrixLayoutDestroy(bLayout);
    cublasLtMatrixLayoutDestroy(cLayout);
    cublasLtMatmulDescDestroy(operationDesc);
    cublasLtMatmulPreferenceDestroy(preference);
    cudaFree(d_a);
    cudaFree(d_b);
    cudaFree(d_c);
    cublasLtDestroy(handle);

    return 0;
}
