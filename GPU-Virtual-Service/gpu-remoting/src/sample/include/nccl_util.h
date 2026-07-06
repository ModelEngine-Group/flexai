#include <arpa/inet.h>
#include <cstdio>
#include <cstdlib>
#include <cuda_runtime.h>
#include <iostream>
#include <nccl.h>
#include <stdint.h>
#include <sys/socket.h>
#include <unistd.h>

#define USE_CONFIG 
#define SERVER_IP "192.168.0.208"
#define SERVER_PORT 15916
#define BUFFER_SIZE 8
#define GPUS_NUM 2
#define CLIENTS_NUM 1

#define CUDACHECK(cmd)                                                         \
    do {                                                                       \
        cudaError_t e = cmd;                                                   \
        if (e != cudaSuccess) {                                                \
            printf("Failed: Cuda error %s:%d '%s'\n", __FILE__, __LINE__,      \
                   cudaGetErrorString(e));                                     \
            exit(EXIT_FAILURE);                                                \
        }                                                                      \
    } while (0)

#define NCCLCHECK(cmd)                                                         \
    do {                                                                       \
        ncclResult_t r = cmd;                                                  \
        if (r != ncclSuccess) {                                                \
            printf("Failed, NCCL error %s:%d '%s'\n", __FILE__, __LINE__,      \
                   ncclGetErrorString(r));                                     \
            exit(EXIT_FAILURE);                                                \
        }                                                                      \
    } while (0)

static void PrintBuffer(void** devBuff) {
    float* hostBuff = (float*)malloc(BUFFER_SIZE * sizeof(float));
    for (int devID = 0; devID < GPUS_NUM; devID++) {
        printf("GPU[%d]", devID);
        CUDACHECK(cudaSetDevice(devID));
        CUDACHECK(cudaMemcpy(hostBuff, devBuff[devID], BUFFER_SIZE * sizeof(float), cudaMemcpyDeviceToHost));

        for(int i = 0; i < BUFFER_SIZE; i++) {
            printf("%f ", hostBuff[i]);
        }
        printf("\n");
    }
    free(hostBuff);
}

static void PrintBuffer(void** devBuff, size_t cnt) {
    float* hostBuff = (float*)malloc(cnt * sizeof(float));
    for (int devID = 0; devID < GPUS_NUM; devID++) {
        printf("GPU[%d]", devID);
        CUDACHECK(cudaSetDevice(devID));
        CUDACHECK(cudaMemcpy(hostBuff, devBuff[devID], cnt * sizeof(float), cudaMemcpyDeviceToHost));

        for(int i = 0; i < cnt; i++) {
            printf("%f ", hostBuff[i]);
        }
        printf("\n");
    }
    free(hostBuff);
}
