# Flexible GPU Virtualization in Cloud (FlexGV)

## Dependencies

- Basic packages: clang, llvm, cmake(>= 3.16) , libboost-all-dev(>= 1.36.0), jemalloc, liblz4-dev, jq, and openssl(>= 1.1.0)

  ```bash
  $ sudo apt-get install llvm clang check libboost-all-dev \
  libjemalloc-dev liblz4-dev libssl-dev jq libelf-dev 
  ```
- CUDA Toolkit: **11.7** (best)

  ```bash
  $ sudo dpkg -i cuda-repo-<distro>_<version>_amd64.deb
  $ sudo apt-key adv --fetch-keys http://developer.download.nvidia.com/compute/cuda/repos/<distro>/x86_64/7fa2af80.pub
  $ sudo apt-get update
  $ sudo apt-get install cuda-11-7
  ```
- UCX: **1.17.0** (best)

  ```bash
  $ CUDA_DIR=/usr/local/cuda-11.7   # Set the path of your CUDA installation
  $ wget https://github.com/openucx/ucx/archive/refs/tags/v1.17.0.zip
  $ unzip v1.17.0.zip
  $ cd ucx-1.17.0
  $ ./autogen.sh
  $ ./contrib/configure-release --enable-mt --enable-gtest --enable-examples --enable-optimizations --disable-logging --disable-debug --disable-assertions --disable-params-check --without-xpmem --without-java --with-cuda=$CUDA_DIR --with-gdrcopy --prefix=$PWD/install
  $ make -j`nproc`
  $ sudo make install
  ```
- ZeroMQ

  ```bash
  $ pip install pyzmq posix_ipc pynvml
  ```

## Build

Before compiling this project, you should set the environment variable `INSTALL_UCX_PATH`

```bash
$ export INSTALL_UCX_PATH=/path/to/ucx/install/
# export INSTALL_UCX_PATH=/home/oem/zwx/rdmalab/ucx-1.17.0/install/
```

Then compile the project with the corresponding parameter on client side and server side.

```bash
$ bash build.sh [option]
```

- Build for client.

  ```bash
  $ bash build.sh -c
  ```

  If you want to build with your CUDA application source files (`.cu`) together, you can copy your files to `./src/sample` folder, just like the sample file: `./src/sample/testKernelLaunch.cu`
- Build for server.

  ```bash
  $ bash build.sh -s
  ```
- Check the command specification by `-h` for more function.

## Usage

The output files (e.g. server application executable, CUDA hook library, your CUDA application executable) will be placed in `./out` folder.

Configuration: use `./config.json` to configure the system (especially, the IP & port of your server node).

Client usage:

```bash
$ LD_PRELOAD=./out/lib64/libcuda_hook.so LD_LIBRARY_PATH=./out/lib64:$LD_LIBRARY_PATH [executable / python XXX.py] 
```

- `[executable]` is the executable of your CUDA application (built from NVCC or Clang)
- `[python XXX.py]` means that you can run a python program

Server usage:

```bash
$ ./out/Server
```

### Example: Running a CUDA application

Command run by the client: 

```bash
$ LD_PRELOAD=./out/lib64/libcuda_hook.so LD_LIBRARY_PATH=./out/lib64:$LD_LIBRARY_PATH ./out/testKernelLaunch
```

If you have put your CUDA application source files to `./src/sample` folder, the executables are placed in `./out` folder.


### Example: Running under PyTorch

Command run by the client: 

```bash
$ LD_PRELOAD=./out/lib64/libcuda_hook.so LD_LIBRARY_PATH=./out/lib64:$LD_LIBRARY_PATH python scripts/quickstart_tutorial.py
```

## API Support

### CUDA Runtime API

Internal API

| API                        | Client | Server | Note |
| -------------------------- | ------ | ------ | ---- |
| __cudaRegisterFunction     | Y      | Y      |      |
| __cudaRegisterVar          | Y      | Y      |      |
| __cudaRegisterFatBinary    | Y      | Y      |      |
| __cudaRegisterFatBinaryEnd | Y      | Y      |      |
| __cudaUnregisterFatBinary  | Y      | Y      |      |

Execution API

| API              | Client | Server | Note |
| ---------------- | ------ | ------ | ---- |
| cudaLaunchKernel | Y      | Y      |      |

Device API

| API                               | Client | Server | Note           |
| --------------------------------- | ------ | ------ | -------------- |
| cudaGetDeviceProperties_v2_v12000 |        |        |                |
| cudaGetDeviceProperties           | Y      | Y      |                |
| cudaDeviceGetAttribute            | Y      | Y      |                |
| cudaGetDeviceCount                | Y      | Y      |                |
| cudaGetDevice                     | Y      | Y      | 目前直接返回 0 |
| cudaSetDevice                     | Y      | Y      |                |
| cudaDeviceSynchronize             | Y      | Y      |                |

Memory API

| API                | Client | Server | Note                                               |
| ------------------ | ------ | ------ | -------------------------------------------------- |
| cudaMemset         | Y      | Y      |                                                    |
| cudaMemcpy         | Y      | Y      | RNDV 协议需要先调用 cudaDeviceSynchronize()        |
| cudaMalloc         | Y      | Y      |                                                    |
| cudaMemcpyToSymbol | Y      | Y      |                                                    |
| cudaFree           | Y      | Y      |                                                    |
| cudaMemsetAsync    | Y      | Y      | 可靠性未保证                                       |
| cudaMemcpyAsync    | Y      | Y      |                                                    |
| cudaMallocHost     | Y      | O      | 直接malloc                                         |
| cudaHostAlloc      | Y      | O      | 上层训练框架只使用default来pin住内存，故直接malloc |
| cudaFreeHost       | Y      | O      | 直接free                                           |
| cudaMemGetInfo     | Y      | Y      |                                                    |

Stream API

| API                             | Client | Server | Note |
| ------------------------------- | ------ | ------ | ---- |
| cudaStreamIsCapturing_v10000    |        |        |      |
| cudaStreamGetCaptureInfo_v10010 |        |        |      |
| cudaStreamIsCapturing           | Y      | Y      |      |
| cudaStreamGetCaptureInfo        | Y      | Y      |      |
| cudaStreamWaitEvent             | Y      | Y      |      |
| cudaStreamSynchronize           | Y      | Y      |      |
| cudaStreamCreateWithFlags       | Y      | Y      |      |
| cudaStreamCreateWithPriority    | Y      | Y      |      |
| cudaStreamCreate                | Y      | Y      |      |
| cudaStreamDestroy               | Y      | Y      |      |

Event API

| API                      | Client | Server | Note                             |
| ------------------------ | ------ | ------ | -------------------------------- |
| cudaEventRecord          | Y      | Y      |                                  |
| cudaEventCreate          | Y      | Y      |                                  |
| cudaEventQuery           | Y      | Y      | 将Server端执行结果返回给Client端 |
| cudaEventDestroy         | Y      | Y      |                                  |
| cudaEventCreateWithFlags | Y      | Y      |                                  |
| cudaEventElapsedTime     | Y      | Y      |                                  |

Other API

| API                                                    | Client | Server | Note                                                     |
| ------------------------------------------------------ | ------ | ------ | -------------------------------------------------------- |
| cudaOccupancyMaxActiveBlocksPerMultiprocessor          | Y      | Y      | 在 CUDA 程序中调用时，CUDA 原生库会自动调用 WithFlags 版 |
| cudaOccupancyMaxActiveBlocksPerMultiprocessorWithFlags | Y      | Y      |                                                          |
| cudalpcOpenMemHandle                                   |        |        |                                                          |
| cudalpcCloseMemHandle                                  |        |        |                                                          |

### CUDA Driver API

| API                                         | Client | Server | Note      |
| ------------------------------------------- | ------ | ------ | --------- |
| cuInit                                      | Y      |        | UCX调用     |
| cuDeviceGetCount                            | Y      |        | UCX调用     |
| cuModuleLoadData                            | Y      |        |           |
| cuModuleGetFunction                         | Y      |        |           |
| cuOccupancyMaxActiveBlocksPerMultiprocessor | Y      |        |           |
| cuGetErrorString                            | Y      |        |           |
| cuCtxGetCurrent                             | Y      |        |           |
| cuModuleUnload                              | Y      |        |           |
| cuDevicePrimaryCtxGetState                  | Y      |        | 目前直接使用缺省值 |
| cuLinkCreate                                | Y      |        |           |
| cuLinkComplete                              | Y      |        |           |
| cuFuncSetAttribute                          | Y      |        |           |
| cuFuncGetAttribute                          | Y      |        |           |

### cuBLAS API

| API                   | Client | Server | Note |
| --------------------- | ------ | ------ | ---- |
| cublasCreate_v2       | Y      | Y      |      |
| cublasSgemm_v2        | Y      | Y      |      |
| cublasDestroy_v2      | Y      | Y      |      |
| cublasSetStream_v2    | Y      | Y      |      |
| cublasSgemmStridedBatched    | Y      | Y      |      |
| cublasSetWorkspace_v2 | Y      | Y      |      |
| cublasSetMathMode     | Y      | Y      |      |
| cublasGetMathMode     | Y      | Y      |      |

### cuBLASLt API

| API                                  | Client | Server | Note                                   |
| ------------------------------------ | ------ | ------ | -------------------------------------- |
| cublasLtCreate                       | Y      | Y      |                                        |
| cublasLtDestroy                      | Y      | Y      |                                        |
| cublasLtMatmulDescCreate             | Y      | Y      |                                        |
| cublasLtMatmulDescDestroy            | Y      | Y      |                                        |
| cublasLtMatmulDescSetAttribute       | Y      | Y      | buf 为 host pointer                    |
| cublasLtMatrixLayoutCreate           | Y      | Y      |                                        |
| cublasLtMatrixLayoutDestroy          | Y      | Y      |                                        |
| cublasLtMatmulPreferenceCreate       | Y      | Y      |                                        |
| cublasLtMatmulPreferenceDestroy      | Y      | Y      |                                        |
| cublasLtMatmulPreferenceSetAttribute | Y      | Y      | buf 为 host pointer                    |
| cublasLtMatmulAlgoGetHeuristic       | Y      | Y      | Client 拷贝次数较多，Server 使用 iov*2 |
| cublasLtMatmulDesc_t                 | Y      | Y      | alpha 和 beta 均为 host float pointer |

### cuDNN API

| API                                                      | Client | Server |         Note         |
| -------------------------------------------------------- | ------ | ------ | -------------------- |
| cudnnCreate                                              |   Y   |   Y   |                      |
| cudnnDestroy                                             |   Y   |   Y   |                      |
| cudnnCreateTensorDescriptor                              |   Y   |   Y   |                      |
| cudnnDestroyTensorDescriptor                             |   Y   |   Y   |                      |
| cudnnGetTensorSizeInBytes                                |   Y   |   Y   |                      |
| cudnnSetTensor4dDescriptor                               |   Y   |   Y   |                      |
| cudnnSetTensorNdDescriptor                               |   Y   |   Y   |                      |
| cudnnSetTensorNdDescriptorEx                             |   Y   |   Y   |                      |
| cudnnCreateTensorTransformDescriptor                     |   Y   |   Y   |                      |
| cudnnSetTensorTransformDescriptor                        |   Y   |   Y   |                      |
| cudnnDestroyTensorTransformDescriptor                    |   Y   |   Y   |                      |
| cudnnInitTransformDest                                   |   Y   |   Y   |                      |
| cudnnTransformTensorEx                                   |   Y   |   Y   |                      |
| cudnnTransformFilter                                     |   Y   |   Y   |                      |
| cudnnCreateFilterDescriptor                              |   Y   |   Y   |                      |
| cudnnSetFilterNdDescriptor                               |   Y   |   Y   |                      |
| cudnnDestroyFilterDescriptor                             |   Y   |   Y   |                      |
| cudnnGetFilterSizeInBytes                                |   Y   |   Y   |                      |
| cudnnGetFoldedConvBackwardDataDescriptors                |   Y   |   Y   |                      |
| cudnnSetStream                                           |   Y   |   Y   |                      |
| cudnnBatchNormalizationBackwardEx                        |   Y   |   Y   |                      |
| cudnnBatchNormalizationForwardTrainingEx                 |   Y   |   Y   |                      |
| cudnnBatchNormalizationForwardInference                  |   Y   |   Y   |                      |
| cudnnBackendCreateDescriptor                             |   Y   |   Y   |                      |
| cudnnBackendDestroyDescriptor                            |   Y   |   Y   |                      |
| cudnnBackendSetAttribute                                 |   Y   |   Y   |                      |
| cudnnBackendGetAttribute                                 |   Y   |   Y   |                      |
| cudnnBackendExecute                                      |   Y   |   Y   | 尚未得到测试程序验证 |
| cudnnBackendFinalize                                     |   Y   |   Y   |                      |
| cudnnGetBatchNormalizationBackwardExWorkspaceSize        |   Y   |   Y   |                      |
| cudnnGetBatchNormalizationForwardTrainingExWorkspaceSize |   Y   |   Y   |                      |
| cudnnGetBatchNormalizationTrainingExReserveSpaceSize     |   Y   |   Y   |                      |
| cudnnCreateConvolutionDescriptor                         |   Y   |   Y   |                      |
| cudnnDestroyConvolutionDescriptor                        |   Y   |   Y   |                      |
| cudnnSetConvolutionGroupCount                            |   Y   |   Y   |                      |
| cudnnSetConvolutionMathType                              |   Y   |   Y   |                      |
| cudnnSetConvolutionNdDescriptor                          |   Y   |   Y   |                      |
| cudnnSetConvolutionReorderType                           |   Y   |   Y   | 尚未得到测试程序验证 |
| cudnnGetConvolutionForwardAlgorithm_v7                   |   Y   |   Y   |                      |
| cudnnGetConvolutionBackwardFilterAlgorithm_v7            |   Y   |   Y   |                      |
| cudnnGetConvolutionBackwardDataAlgorithm_v7              |   Y   |   Y   |                      |
| cudnnGetConvolutionForwardWorkspaceSize                  |   Y   |   Y   |                      |
| cudnnConvolutionForward                                  |   Y   |   Y   |                      |
| cudnnGetConvolutionBackwardDataWorkspaceSize             |   Y   |   Y   |                      |
| cudnnConvolutionBackwardFilter                           |   Y   |   Y   |                      |
| cudnnGetConvolutionBackwardFilterWorkspaceSize           |   Y   |   Y   |                      |
| cudnnConvolutionBackwardData                             |   Y   |   Y   |                      |

## NCCL API

| API                      | Client | Server | Note                                  |
| ------------------------ | ------ | ------ | ------------------------------------- |
| ncclGroupStart           | Y      | Y      |                                       |
| ncclGroupEnd             | Y      | Y      |                                       |
| ncclGroupSimulateEnd     |        |        | v2.22版本新增，暂不考虑支持                      |
| ncclCommInitRank         | Y      | Y      |                                       |
| ncclCommInitRankConfig   | Y      | Y      |                                       |
| ncclCommInitAll          | Y      | Y      |                                       |
| ncclCommSplit            | Y      | Y      | 暂无样例程序可验证                             |
| ncclCommFinalize         | Y      | Y      |                                       |
| ncclCommDestroy          | Y      | Y      |                                       |
| ncclCommGetAsyncError    | Y      | Y      |                                       |
| ncclCommAbort            | Y      | Y      |                                       |
| ncclCommCount            | Y      | Y      |                                       |
| ncclCommCuDevice         | Y      | Y      |                                       |
| ncclCommUserRank         | Y      | Y      |                                       |
| ncclCommRegister         | Y      | Y      |                                       |
| ncclCommDeregister       | Y      | Y      |                                       |
| ncclMemAlloc             | Y      | Y      |                                       |
| ncclMemFree              | Y      | Y      |                                       |
| ncclRedOpDestroy         | Y      | Y      |                                       |
| ncclRedOpCreatePreMulSum | Y      | Y      | `ncclScalarHostImmediate` 情形未有样例程序可验证 |
| ncclAllReduce            | Y      | Y      |                                       |
| ncclAllGather            | Y      | Y      |                                       |
| ncclReduceScatter        | Y      | Y      |                                       |
| ncclReduce               | Y      | Y      |                                       |
| ncclBroadcast            | Y      | Y      |                                       |
| ncclSend                 | Y      | Y      |                                       |
| ncclRecv                 | Y      | Y      |                                       |
| ncclGetUniqueId          | Y      | Y      |                                       |
| ncclGetVersion           | Y      | Y      |                                       |
| ncclGetErrorString       | Y      | O      | 直接返回                                  |
| ncclGetLastError         | Y      | O      | 直接返回                                  |


## References

- [DEBE](https://github.com/yzr95924/DEBE)
- [GVirtuS](https://github.com/gvirtus/GVirtuS)
- [cuda_hook](https://github.com/Bruce-Lee-LY/cuda_hook/tree/master)
- [UCX](https://github.com/openucx/ucx)
