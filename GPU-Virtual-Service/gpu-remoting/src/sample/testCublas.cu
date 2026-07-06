/**
* 30 84 138 
* 24 69 114 
* 18 54 90
*/
#include <iostream>
#include <cublas_v2.h>

int main() {
    cublasHandle_t handle;
    const int N = 3;

    // 初始化 cuBLAS 句柄
    cublasCreate(&handle);

    // 分配和初始化矩阵
    float a[N*N] = {1.0, 2.0, 3.0,
                    4.0, 5.0, 6.0,
                    7.0, 8.0, 9.0};
    float b[N*N] = {9.0, 8.0, 7.0,
                    6.0, 5.0, 4.0,
                    3.0, 2.0, 1.0};
    float c[N*N] = {0};

    float *d_a, *d_b, *d_c;
    cudaMalloc((void **)&d_a, sizeof(a));
    cudaMalloc((void **)&d_b, sizeof(b));
    cudaMalloc((void **)&d_c, sizeof(c));

    // 将数据复制到设备
    cudaMemcpy(d_a, a, sizeof(a), cudaMemcpyHostToDevice);
    cudaMemcpy(d_b, b, sizeof(b), cudaMemcpyHostToDevice);

    size_t workspace_size = 10 * 1024 * 1024; // 10 MB
    void* workspace;
    cudaMalloc(&workspace, workspace_size);
    cublasSetWorkspace_v2(handle, workspace, workspace_size);

    // 执行矩阵乘法
    const float alpha = 1.0f;
    const float beta = 0.0f;
    cublasSgemm(handle, CUBLAS_OP_T, CUBLAS_OP_T, N, N, N, &alpha, d_a, N, d_b, N, &beta, d_c, N);

    // 将结果复制回主机
    cudaMemcpy(c, d_c, sizeof(c), cudaMemcpyDeviceToHost);

    // 打印结果
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            std::cout << c[i * N + j] << " ";
        }
        std::cout << std::endl;
    }

    // 清理
    cudaFree(d_a);
    cudaFree(d_b);
    cudaFree(d_c);
    cudaFree(workspace);
    cublasDestroy(handle);

    return 0;
}