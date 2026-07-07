#include <assert.h>
#include <cuda_runtime.h>
#include <iostream>
#include <fstream>

int main(int argc, char* argv[]) {
    // float time_elapsed = 0;
    cudaEvent_t start,end;
    cudaEventCreate(&start);
    cudaEventCreateWithFlags(&end,0);
    printf("start = %p, end = %p\n",start,end);
    
    cudaEventRecord(start,0);
    int *dev_A;
    cudaMalloc(&dev_A,8);
    cudaEventRecord(end,0);

    cudaEventQuery(start);
    cudaEventQuery(end);

    cudaEventDestroy(start);
    cudaEventDestroy(end);
    return 0;
}