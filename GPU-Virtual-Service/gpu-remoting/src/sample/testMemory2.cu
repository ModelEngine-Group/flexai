// main.cu
#include <assert.h>
#include <cuda_runtime.h>
#include <iostream>
#include <fstream>
#include <sys/time.h>
//#define N 4096


int main(int argc, char* argv[]) {
    int nelem = 1048576;
    size_t bytes = nelem * sizeof(int);

    
    size_t freeMem, totalMem;
    cudaMemGetInfo(&freeMem, &totalMem);
    printf("freeMem: %lu, totalMem: %lu\n", freeMem, totalMem);

    int *A_arr, *B_arr, *C_arr;
    // testcodes: https://github.com/NVIDIA/cuda-samples/blob/e8568c417356f7e66bb9b7130d6be7e55324a519/Samples/2_Concepts_and_Techniques/inlinePTX/inlinePTX.cu#L82
    cudaMallocHost((void**)&A_arr, bytes);
    //cudaMallocHost((void**)&B_arr, bytes);
    cudaHostAlloc((void**)&B_arr, bytes, cudaHostAllocDefault);
    // A_arr = (int*)malloc(bytes);
    // B_arr = (int*)malloc(bytes);
    C_arr = (int*)malloc(bytes);

    for (int i = 0; i < nelem; i++) {
        A_arr[i] = i;
        B_arr[i] = 2;
        C_arr[i] = 0;
    }
    int idx = nelem - 2;
    printf("A_arr addr = %p, A_arr[%d] = %d\n", A_arr, idx, A_arr[idx]);
    printf("B_arr addr = %p, B_arr[%d] = %d\n", B_arr, idx, B_arr[idx]);



    
    return 0;
}
