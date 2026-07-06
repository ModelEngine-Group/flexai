#include <assert.h>
#include <cuda_runtime.h>
#include <iostream>
#include <fstream>
#include <sys/time.h>

#define DATA_SIZE (64ULL << 20ULL) // 64MB

__global__ void simpleKernel(char *ptr, int sz, char val) {
  int idx = blockIdx.x * blockDim.x + threadIdx.x;
  for (; idx < sz; idx += (gridDim.x * blockDim.x)) {
    ptr[idx] = val;
  }
}

int main() {
    int blocks = 0;
    int threads = 128;
    cudaDeviceProp prop;
    // cudaStream_t stream;
    // char id = 'x';
    
    cudaGetDeviceProperties(&prop, 0);
    std::cout << "SM count: " << prop.multiProcessorCount << std::endl;
    cudaOccupancyMaxActiveBlocksPerMultiprocessor(&blocks, simpleKernel, threads, 0);
    std::cout << "Max blocks per SM: " << blocks << std::endl;
    
    cudaOccupancyMaxActiveBlocksPerMultiprocessorWithFlags(&blocks, simpleKernel, threads, 0, cudaOccupancyDisableCachingOverride);
    std::cout << "Max blocks per SM (no caching): " << blocks << std::endl;

    return 0;
}