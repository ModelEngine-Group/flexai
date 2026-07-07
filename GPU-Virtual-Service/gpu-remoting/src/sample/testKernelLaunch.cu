#include <assert.h>
#include <iostream>
#include <fstream>
#include <sys/time.h>
//#define N 1024
//#define N 512


__global__ void add(const int *A, const int *B, int *C, size_t n) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n)
        C[i] = A[i] + B[i];
}

__global__ void sub(const int *A, const int *B, int *C, size_t n) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n)
        C[i] = A[i] + B[i];
}

int main(int argc, char* argv[]) {
    // cudaDeviceSynchronize();
    cudaSetDevice(0);
    int testSize = 4096;
    const char* logDir;
    if (argc == 3) {
        testSize = atoi(argv[2]);
        logDir = argv[1];
    }
    if (argc != 3 && argc != 1){
        printf("Usage: %s [path to the log file] [testSize]\n", argv[0]);
        return 0;
    }
    int* A_arr = (int*)malloc(testSize);
    int* B_arr = (int*)malloc(testSize);
    int* C_arr = (int*)malloc(testSize);
    int num = testSize / sizeof(int);
    struct timeval timestart;
    struct timeval timeend;
    gettimeofday(&timestart, NULL);
    for (size_t i = 0; i < num; i++)
        A_arr[i] = i, B_arr[i] = 1, C_arr[i] = 0;

    int *dev_A, *dev_B, *dev_C;
    assert(cudaSuccess == cudaMalloc(&dev_A, testSize));
    assert(cudaSuccess == cudaMalloc(&dev_B, testSize));
    assert(cudaSuccess == cudaMalloc(&dev_C, testSize));
    assert(cudaSuccess ==
           cudaMemcpy(dev_A, A_arr, testSize, cudaMemcpyHostToDevice));
    assert(cudaSuccess ==
           cudaMemcpy(dev_B, B_arr, testSize, cudaMemcpyHostToDevice));
    assert(cudaSuccess ==
           cudaMemcpy(dev_C, C_arr, testSize, cudaMemcpyHostToDevice));
    std::cout << "address of dev_A = " << dev_A << ", dev_B = " << dev_B
              << ", dev_C = " << dev_C << std::endl;

    size_t blockDim_x = 512;
    //size_t blockDim_x = 256;
    size_t nr_block = (num + blockDim_x - 1) / blockDim_x;
    std::cout << "nr_block = " << nr_block << ", blockDim_x = " << blockDim_x
              << std::endl;
    add<<<nr_block, blockDim_x>>>(dev_A, dev_B, dev_C, num);
    std::cout << "before cudaMemcpy, address of dev_C = " << dev_C << std::endl;
    cudaDeviceSynchronize();
    assert(cudaSuccess ==
           cudaMemcpy(C_arr, dev_C, testSize, cudaMemcpyDeviceToHost));

    cudaFree(dev_A);
    cudaFree(dev_B);
    cudaFree(dev_C);

    std::cout << "C = {";
    for (size_t i = num - 10; i < num; i++)
        std::cout << C_arr[i] << ", ";
    std::cout << "}" << std::endl;
    gettimeofday(&timeend, NULL);
    long diff = 1000000 * (timeend.tv_sec - timestart.tv_sec) +
                timeend.tv_usec - timestart.tv_usec;
    double msTotalTime = 1.0f * diff / 1000.0;
    printf("Total time = %lf ms\n", msTotalTime);
    if (argc == 3) {
        std::ofstream outFile(logDir, std::ios::out | std::ios::app); 
        outFile << msTotalTime << std::endl;
        outFile.close();
    }
    return 0;
}
