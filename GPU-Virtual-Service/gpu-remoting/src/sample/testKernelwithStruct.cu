#include <assert.h>
#include <iostream>
#include <fstream>
#include <sys/time.h>

typedef struct {
    float a;
    float b;
    float *z; // pointer to device memory
    float *w; // another pointer to device memory
} NestedStruct;

typedef struct {
    int x;
    float y;
    NestedStruct nested; 
    float *host_data; // pointer to host memory
} MyStruct;

__global__ void myKernel(MyStruct myStruct, int offset) {
    int idx = threadIdx.x + blockIdx.x * blockDim.x;
    myStruct.nested.z[idx] = (myStruct.nested.z[idx] * myStruct.x * myStruct.y * myStruct.nested.a * myStruct.nested.b) + offset;
    myStruct.nested.w[idx] = myStruct.nested.z[idx] + myStruct.nested.w[idx];
}

int main(int argc, char* argv[]) {
    // 定义并初始化结构体
    MyStruct h_myStruct;
    h_myStruct.x = 2;
    h_myStruct.y = 3.5;
    h_myStruct.nested.a = 1.1; // 初始化嵌套结构体成员
    h_myStruct.nested.b = 2.2;

    // 在设备上分配内存并初始化
    float h_z[10] = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0};
    float h_w[10] = {10.0, 9.0, 8.0, 7.0, 6.0, 5.0, 4.0, 3.0, 2.0, 1.0};
    float *d_z, *d_w;
    cudaMalloc((void**)&d_z, 10 * sizeof(float));
    cudaMalloc((void**)&d_w, 10 * sizeof(float));
    cudaMemcpy(d_z, h_z, 10 * sizeof(float), cudaMemcpyHostToDevice);
    cudaMemcpy(d_w, h_w, 10 * sizeof(float), cudaMemcpyHostToDevice);

    // 将设备指针赋值给嵌套结构体
    h_myStruct.nested.z = d_z;
    h_myStruct.nested.w = d_w;

    // 在主机上分配内存并初始化
    float h_host_data[10] = {0.0};
    h_myStruct.host_data = h_host_data;

    // 定义offset
    int offset = 5;

    // 将结构体作为参数传递给 Kernel 函数，并传递 offset
    myKernel<<<1, 10>>>(h_myStruct, offset);

    // 同步并检查错误
    cudaDeviceSynchronize();

    // 将结果从设备内存复制回主机内存
    cudaMemcpy(h_z, d_z, 10 * sizeof(float), cudaMemcpyDeviceToHost);
    cudaMemcpy(h_w, d_w, 10 * sizeof(float), cudaMemcpyDeviceToHost);

    // 打印结果
    printf("Results in z:\n");
    for (int i = 0; i < 10; ++i) {
        printf("%f ", h_z[i]);
    }
    printf("\n");

    printf("Results in w:\n");
    for (int i = 0; i < 10; ++i) {
        printf("%f ", h_w[i]);
    }
    printf("\n");

    // 释放设备内存
    cudaFree(d_z);
    cudaFree(d_w);

    return 0;
}