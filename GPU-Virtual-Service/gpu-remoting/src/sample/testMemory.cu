/**
* freeMem: 16603348992, totalMem: 16928342016
* B_arr addr: 0x7f06c7ffe010, data: {16843009,16843009,16843009,16843009,16843009,16843009,16843009,16843009,16843009,16843009}
* C_arr addr: 0x7f06a7ffd010, data: {134217718,134217719,134217720,134217721,134217722,134217723,134217724,134217725,134217726,134217727}
* Total time = 370.274000 ms
*/
#include <assert.h>
#include <cuda_runtime.h>
#include <iostream>
#include <fstream>
#include <sys/time.h>
//#define N 4096


int main(int argc, char* argv[]) {
    cudaDeviceSynchronize();
    int testSize = 512 * 1024 * 1024; // 4 MB 测试 RNDV 
    const char* logDir;
    if (argc == 3) {
        testSize = atoi(argv[2]);
        logDir = argv[1];
    }
    if (argc != 3 && argc != 1){
        printf("Usage: %s [testSize] [path to the log file]\n", argv[0]);
        return 0;
    }
    size_t freeMem, totalMem;
    cudaMemGetInfo(&freeMem, &totalMem);
    printf("freeMem: %lu, totalMem: %lu\n", freeMem, totalMem);

    int* A_arr = (int*)malloc(testSize);
    int* B_arr = (int*)malloc(testSize);
    int* C_arr = (int*)malloc(testSize);
    int num = testSize / sizeof(int);
    for (size_t i = 0; i < num; i++)
        A_arr[i] = i, B_arr[i] = 0, C_arr[i] = 0;

    struct timeval timestart;
    struct timeval timeend;
    gettimeofday(&timestart, NULL);

    int *dev_A, *dev_B;
    
    assert(cudaSuccess == cudaMalloc(&dev_A, testSize));
    assert(cudaSuccess == cudaMalloc(&dev_B, testSize));
    assert(cudaSuccess ==
           cudaMemcpy(dev_A, A_arr, testSize, cudaMemcpyHostToDevice));
    assert(cudaSuccess ==
           cudaMemset(dev_B, 1, testSize)); 
    // assert(cudaSuccess ==
    //        cudaMemcpy(dev_B, B_arr, testSize, cudaMemcpyHostToDevice));
   // cudaDeviceSynchronize();
    assert(cudaSuccess ==
           cudaMemcpy(B_arr, dev_B, testSize, cudaMemcpyDeviceToHost));
    assert(cudaSuccess ==
           cudaMemcpy(C_arr, dev_A, testSize, cudaMemcpyDeviceToHost));
    //cudaDeviceSynchronize();

    printf("B_arr addr: %p, data: {", B_arr);


    for (size_t i = max(num - 10, 0); i < num; i++) //  16843009 = 0000 0001 0000 0001 0000 0001 0000 0001
        printf("%d%s", B_arr[i], (i == num - 1) ? "}\n" : ",");
    
    printf("C_arr addr: %p, data: {", C_arr);
    for (size_t i = max(num - 10, 0); i < num; i++)
        printf("%d%s", C_arr[i], (i == num - 1) ? "}\n" : ",");

    cudaFree(dev_A);
    cudaFree(dev_B);
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
