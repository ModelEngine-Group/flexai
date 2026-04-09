/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#include <cuda.h>
#include <dlfcn.h>
#include <nvml_compat.h>
#include <sys/time.h>
#include "gpu_manager.h"
#include "log.h"

using namespace xpu;

int GpuManager::InitXpu()
{
  static decltype(cuInit) *const rawCuInit = reinterpret_cast<decltype(cuInit) *>(dlsym(RTLD_NEXT, "cuInit"));
  if (rawCuInit == nullptr) {
    return RET_FAIL;
  }
  CUresult ret = rawCuInit(0);
  if (ret != CUDA_SUCCESS) {
    return RET_FAIL;
  }
  return pid_.Initialize();
}

int GpuManager::CurrentDevice() {
  CUdevice dev;
  auto ret = cuCtxGetDevice(&dev);
  if (ret != CUDA_SUCCESS) {
    return INVALID_DEVICE_IDX;
  }
  return GetCudaDeviceId(dev);
}

nvmlDevice_t GpuManager::GetNvmlHandle(int idx) {
  if (!CheckDeviceIndex(idx)) {
    return INVALID_NVML_HANDLE;
  }
  std::call_once(handleMapInit_, &GpuManager::InitDeviceMap, this);
  return nvmlDevice_[idx];
}

int GpuManager::GetCudaDeviceId(CUdevice dev) {
  std::call_once(handleMapInit_, &GpuManager::InitDeviceMap, this);
  auto iter = cuDevice_.find(dev);
  if (iter == cuDevice_.end()) {
    return INVALID_DEVICE_IDX;
  }
  return iter->second;
}

int GpuManager::InitDeviceMap() {
  int count;
  CUresult res = cuDeviceGetCount(&count);
  if (res != CUDA_SUCCESS) {
    return RET_FAIL;
  }
  if (count < 0 || count > XpuManager::MAX_DEVICE_COUNT) {
    return RET_FAIL;
  }

  nvmlReturn_t ret = nvmlInit();
  if (ret != NVML_SUCCESS) {
    return RET_FAIL;
  }

  for (int i = 0; i < count; i++) {
    CUdevice dev;
    res = cuDeviceGet(&dev, i);
    if (res != CUDA_SUCCESS) {
      return RET_FAIL;
    }

    nvmlDevice_t nvmlDevice;
    ret = nvmlDeviceGetHandleByIndex(i, &nvmlDevice);
    if (ret != NVML_SUCCESS) {
      return RET_FAIL;
    }

    cuDevice_[dev] = i;
    nvmlDevice_[i] = nvmlDevice;
  }
  devCnt_ = count;
  return RET_SUCC;
}

int GpuManager::MemoryUsed(size_t& used) {
  nvmlDevice_t dev = GetCurrNvmlHandle();
  if (dev == GpuManager::INVALID_NVML_HANDLE) {
    return RET_FAIL;
  }

  unsigned int pidCount = MAX_PIDS;
  nvmlProcessInfo_t memInfos[MAX_PIDS] = {};
  nvmlReturn_t ret = nvmlDeviceGetComputeRunningProcesses(dev, &pidCount, memInfos);
  if (ret != NVML_SUCCESS || pidCount > MAX_PIDS) {
    return RET_FAIL;
  }

  used = 0;
  for (unsigned int i = 0; i < pidCount; i++) {
    int containerPid = pid_.GetContainerPid(memInfos[i].pid);
    if (containerPid != pid_.INVALID_PID) {
      used += memInfos[i].usedGpuMemory;
    }
  }
  return RET_SUCC;
}

int GpuManager::ComputingPowerUsed(int idx, unsigned int& used) {
  nvmlDevice_t dev = GetNvmlHandle(idx);
  if (dev == GpuManager::INVALID_NVML_HANDLE) {
    return RET_FAIL;
  }

  unsigned int runProcNum = MAX_PIDS;
  nvmlProcessInfo_t runProcInfos[MAX_PIDS] = {};
  nvmlReturn_t ret = nvmlDeviceGetComputeRunningProcesses(dev, &runProcNum, runProcInfos);
  if (ret != NVML_SUCCESS || runProcNum > MAX_PIDS) {
    return RET_FAIL;
  }

  struct timeval cur;
  gettimeofday(&cur, nullptr);
  uint64_t checkTime = (cur.tv_sec - SHORT_PROC_UTIL_PERIOD) * MICROSEC + cur.tv_usec;
  unsigned int procNum = MAX_PIDS;
  nvmlProcessUtilizationSample_t procSample[MAX_PIDS] = {};
  ret = nvmlDeviceGetProcessUtilization(dev, procSample, &procNum, checkTime);
  if (ret != NVML_SUCCESS || procNum > MAX_PIDS) {
    return RET_FAIL;
  }
  if (runProcNum != procNum) {
    procNum = MAX_PIDS;
    checkTime = (cur.tv_sec - LONG_PROC_UTIL_PERIOD) * MICROSEC + cur.tv_usec;
    ret = nvmlDeviceGetProcessUtilization(dev, procSample, &procNum, checkTime);
    if (ret != NVML_SUCCESS || procNum > MAX_PIDS) {
      return RET_FAIL;
    }
  }

  unsigned int rate = 0;
  for (unsigned int i = 0; i < procNum; i++) {
    if (procSample[i].timeStamp < checkTime) {
      return RET_FAIL;
    }
    int containerPid = pid_.GetContainerPid(static_cast<int>(procSample[i].pid));
    if (containerPid != pid_.INVALID_PID) {
      rate += procSample[i].smUtil;
    }
  }
  used = std::clamp(rate, PERCENT_MIN, PERCENT_MAX);
  return RET_SUCC;
}
