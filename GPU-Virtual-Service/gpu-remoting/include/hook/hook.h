#ifndef GV_HOOK_H
#define GV_HOOK_H

#include <cstdint>
#include <memory>
#include <cuda.h>
#include <cuda_runtime.h>
#include <cublas_v2.h>
#include <cublasLt.h>
#include <cudnn.h>
#include <nvml.h>
#include <nccl.h>
#include <dlfcn.h>

// #include "fatBinaryCtl.h"
#include "fatBinary.h"
#include "../configure.h"
#include "../ucpConnection.h"
#include "../shmqueue/shmUtil.h"
#include "../clientEndpoint.h"

#define HOOK_LOG_TAG "GV-Hook"

extern int mainDevIdx; // different ranks(processes) may use different devices
extern std::vector<ClientEndpoint*> clientEpList;
extern std::vector<bool> threadValidList;

extern Configure* config_;
extern UCPConnection* connectionObj;
extern SharedMemoryOpt* shmOpt;
extern GPUidMap* gpuIdMap;
extern std::once_flag initFlag;
extern std::once_flag registerFlag;
extern std::vector<RegisterIOV*> regIOVList;
extern std::vector<KernelInfo_t*> registeredKernels;
extern robin_hood::unordered_flat_map<uint64_t, KernelInfo_t*> mapHost2KernelInfo;

extern bool isReConnected;
extern std::mutex reConnectMutex;
extern std::shared_mutex threadSharedMutex;
extern int processID; // the process ID of the current process
extern int threadNum; // how many sub-threads have been created in this process
extern int commDevIdx; // the device index for current process's communicator
extern thread_local int threadID; // e.g, 1641432
extern thread_local int ttID; // e.g, 1, 2, 3, ...
extern thread_local ClientEndpoint* clientEpObj; // the client endpoint object for the current thread
extern thread_local int myDevIdx;
extern thread_local int lastReqType;
extern thread_local bool isTraining;
extern thread_local bool batchCollected;
extern thread_local int curTensorIdx;
extern thread_local size_t curIter;
extern thread_local std::vector<TensorInfo_t> tensorByteList;

void ConnectToDispatcher(Configure config);
void SwitchClientEp(int dev, bool threadInit = false);
void Intialize();
void DestoryResources();

inline void HookLog(const char* func, bool checkClientEp = true, int debugLevel = LOG_DEBUG) {
    if (checkClientEp || (ttID > 0 && threadValidList[ttID - 1] == false)){ // check if a new thread, or if the clientEp for myDevIdx has been created
        SwitchClientEp(myDevIdx, clientEpObj == nullptr);
    }
    tool::Logging(debugLevel, HOOK_LOG_TAG, "[pid:%d, tid:%d, ttid:%d] ======== %s ========\n", processID, threadID, ttID, func);
}

void CheckTensors(int reqType);
bool CheckIteration(void* dst, size_t size);

#endif