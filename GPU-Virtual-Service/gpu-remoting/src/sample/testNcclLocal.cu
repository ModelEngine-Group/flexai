#include "./include/nccl_util.h"

int main(int argc, char *argv[]) {

  // managing 4 devices
  int nDev = GPUS_NUM;
  int devs[GPUS_NUM];
  for (int i = 0; i < nDev; ++i) devs[i] = i;

  ncclComm_t comms[GPUS_NUM];

  // allocating and initializing device buffers
  float **sendbuff = (float **)malloc(nDev * sizeof(float *));
  float **recvbuff = (float **)malloc(nDev * sizeof(float *));
  float **recvbuff2 = (float **)malloc(nDev * sizeof(float *));
  void *sendRegHandle[nDev];
  void *recvRegHandle[nDev];
  cudaStream_t *s = (cudaStream_t *)malloc(sizeof(cudaStream_t) * nDev);
  float* tmpbuff = (float*)malloc(BUFFER_SIZE * sizeof(float));
  for (int i = 0; i < BUFFER_SIZE; i++) tmpbuff[i] = 1;

  for (int i = 0; i < nDev; ++i) {
    CUDACHECK(cudaSetDevice(i));

    NCCLCHECK(ncclMemAlloc((void **)sendbuff + i, BUFFER_SIZE * sizeof(float)));
    NCCLCHECK(ncclMemAlloc((void **)recvbuff + i, BUFFER_SIZE * sizeof(float)));

    // CUDACHECK(cudaMalloc((void**)sendbuff + i, size * sizeof(float)));
    NCCLCHECK(ncclMemAlloc((void **)recvbuff2 + i, BUFFER_SIZE * sizeof(float)));

    CUDACHECK(cudaMemcpy(sendbuff[i], tmpbuff, BUFFER_SIZE * sizeof(float), cudaMemcpyHostToDevice));

    CUDACHECK(cudaMemset(recvbuff[i], 0, BUFFER_SIZE * sizeof(float)));
    CUDACHECK(cudaMemset(recvbuff2[i], 0, BUFFER_SIZE * sizeof(float)));
    CUDACHECK(cudaStreamCreate(s + i));
  }

  // initializing NCCL and preparing custom operation
  NCCLCHECK(ncclCommInitAll(comms, nDev, devs));
  float scalar_1 = 2.0f, scalar_2 = 3.0f;
  float *d_scalar;
  //   CUDACHECK(cudaMalloc(&d_scalar, sizeof(float)));
  NCCLCHECK(ncclMemAlloc((void **)&d_scalar, sizeof(float)));
  CUDACHECK(
      cudaMemcpy(d_scalar, &scalar_2, sizeof(float), cudaMemcpyHostToDevice));
  ncclRedOp_t customOp_1[nDev], customOp_2[nDev];
  for (int i = 0; i < nDev; ++i) {
    NCCLCHECK(ncclCommRegister(comms[i], sendbuff[i], BUFFER_SIZE * sizeof(float),
                               sendRegHandle + i));
    NCCLCHECK(ncclCommRegister(comms[i], recvbuff[i], BUFFER_SIZE * sizeof(float),
                               recvRegHandle + i));
    NCCLCHECK(ncclRedOpCreatePreMulSum(customOp_1 + i, &scalar_1, ncclFloat,
                                       ncclScalarHostImmediate, comms[i]));
    NCCLCHECK(ncclRedOpCreatePreMulSum(customOp_2 + i, d_scalar, ncclFloat,
                                       ncclScalarDevice, comms[i]));
    printf("CustomOp_1: %u, CustomOp_2: %u\n", customOp_1[i], customOp_2[i]);
  }

  printf("Before AllGather:\n");
  PrintBuffer((void**)sendbuff); 

  // calling NCCL communication API. Group API is required when using
  // multiple devices per thread
  NCCLCHECK(ncclGroupStart());
  for (int i = 0; i < nDev; ++i) {
    NCCLCHECK(ncclAllReduce((const void *)sendbuff[i], (void *)recvbuff[i],
                            BUFFER_SIZE, ncclFloat, customOp_1[i], comms[i], s[i]));
  }
  NCCLCHECK(ncclGroupEnd());

  // synchronizing on CUDA streams to wait for completion of NCCL operation
  for (int i = 0; i < nDev; ++i) {
    CUDACHECK(cudaSetDevice(i));
    CUDACHECK(cudaStreamSynchronize(s[i]));
  }

//   NCCLCHECK(ncclGroupStart());
//   for (int i = 0; i < nDev; ++i) {
//     NCCLCHECK(ncclAllReduce((const void *)sendbuff[i], (void *)recvbuff2[i],
//                             size, ncclFloat, customOp_2[i], comms[i], s[i]));
//   }
//   NCCLCHECK(ncclGroupEnd());

//   // synchronizing on CUDA streams to wait for completion of NCCL operation
//   for (int i = 0; i < nDev; ++i) {
//     CUDACHECK(cudaSetDevice(i));
//     CUDACHECK(cudaStreamSynchronize(s[i]));
//   }

  printf("AllGather result:\n");
  PrintBuffer((void**)recvbuff);

  // free device buffers
  for (int i = 0; i < nDev; ++i) {
    CUDACHECK(cudaSetDevice(i));
    NCCLCHECK(ncclCommDeregister(comms[i], sendRegHandle[i]));
    NCCLCHECK(ncclCommDeregister(comms[i], recvRegHandle[i]));
    NCCLCHECK(ncclMemFree(sendbuff[i]));
    NCCLCHECK(ncclMemFree(recvbuff[i]));
    NCCLCHECK(ncclMemFree(recvbuff2[i]));
    // CUDACHECK(cudaFree(sendbuff[i]));
    // CUDACHECK(cudaFree(recvbuff2[i]));
  }
  

  // finalizing NCCL
  for (int i = 0; i < nDev; ++i) {
    ncclRedOpDestroy(customOp_1[i], comms[i]);
    ncclRedOpDestroy(customOp_2[i], comms[i]);
  }

  for (int i =0; i < nDev; ++i) {
    // Finalize the NCCL communicator
    NCCLCHECK(ncclCommFinalize(comms[i]));

    // Check for communicator state
    ncclResult_t asyncErr;
    do {
      NCCLCHECK(ncclCommGetAsyncError(comms[i], &asyncErr));
    } while (asyncErr == ncclInProgress);

    if (asyncErr == ncclSuccess) {
      printf("Communicator successfully finalized.\n");
      NCCLCHECK(ncclCommDestroy(comms[i]));
    } else {
      printf("Failed to finalize communicator.\n");
    }
  }

  // ncclUniqueId commId;
  // NCCLCHECK(ncclGetUniqueId(&commId));
  // ncclConfig_t config = NCCL_CONFIG_INITIALIZER;
  // config.netName = "Socket";
  // ncclComm_t newcomm[nDev];
  // NCCLCHECK(ncclGroupStart());
  // for (int i = 0; i < nDev; ++i) {
  //   NCCLCHECK(ncclCommInitRankConfig(newcomm + i, nDev, commId, i, &config));
  // }
  // NCCLCHECK(ncclGroupEnd());

  // for (int i = 0; i < nDev; ++i) {
  //   NCCLCHECK(ncclCommDestroy(newcomm[i]));
  // }

  printf("Success \n");
  return 0;
}