#include "monitor.h"
#include <filesystem>
#include <iostream>
#include <map>
#include <getopt.h>
#include "resource_config.h"
#include "log.h"

using namespace std;

namespace xpu {
int FillProcMem(VxpuInfo &info, PidManager &pids, nvmlDevice_t dev) {
  nvmlProcessInfo_t procInfos[MAX_PIDS] = {};
  uint32_t infoSize = MAX_PIDS;
  auto ret = nvmlDeviceGetComputeRunningProcesses(dev, &infoSize, procInfos);
  if (ret != NVML_SUCCESS) {
    return RET_FAIL;
  }
  const auto myPid = getpid();
  for (uint32_t i = 0; i < infoSize; i++) {
    auto &pidInfo = procInfos[i];
    auto pid = pids.GetContainerPid(pidInfo.pid);
    if (pid == PidManager::INVALID_PID || pid == myPid) {
      continue;
    }
    auto &proc = info.processes[pid];
    proc.memory = pidInfo.usedGpuMemory;
    info.memory += pidInfo.usedGpuMemory;
  }
  return RET_SUCC;
}

int FillProcCore(VxpuInfo &info, PidManager &pids, nvmlDevice_t dev, size_t timestamp) {
  nvmlProcessUtilizationSample_t procInfos[MAX_PIDS] = {};
  uint32_t sampleSize = MAX_PIDS;
  auto ret = nvmlDeviceGetProcessUtilization(dev, procInfos, &sampleSize, timestamp);
  if (ret == NVML_ERROR_NOT_FOUND) {
    return RET_SUCC;
  }
  if (ret != NVML_SUCCESS) {
    return RET_FAIL;
  }
  const auto myPid = getpid();
  for (uint32_t i = 0; i < sampleSize; i++) {
    auto &sample = procInfos[i];
    auto pid = pids.GetContainerPid(sample.pid);
    if (pid == PidManager::INVALID_PID || pid == myPid) {
      continue;
    }
    auto &proc = info.processes[pid];
    proc.core = sample.smUtil;
    info.core += sample.smUtil;
  }
  return RET_SUCC;
}

int FillVgpuInfo(VxpuInfo &info, nvmlDevice_t &dev) {
  nvmlReturn_t ret = nvmlDeviceGetHandleByIndex(info.id, &dev);
  if (ret != NVML_SUCCESS) {
    return RET_FAIL;
  }

  if (info.memoryQuota == 0) {
    nvmlMemory_t memInfo;
    ret = nvmlDeviceGetMemoryInfo(dev, &memInfo);
    if (ret != NVML_SUCCESS) {
      return RET_FAIL;
    }
    info.memoryQuota = memInfo.total;
  }
  return RET_SUCC;
}

int FillProcInfo(VxpuInfo &info, nvmlDevice_t dev, PidManager &pids, size_t timestamp) {
  int ret = FillProcMem(info, pids, dev);
  if (ret != RET_SUCC) {
    return ret;
  }
  ret = FillProcCore(info, pids, dev, timestamp);
  if (ret != RET_SUCC) {
    return ret;
  }
  return RET_SUCC;
}

int CudaMonitorMain(int argc, char *argv[]) {
  Args args;
  GpuManager gpu;
  PidManager &pids = gpu.PidsMap();
  ResourceConfig config(gpu);
  if (ParseArgs(args, argc, argv) != RET_SUCC) {
    return RET_FAIL;
  }
  const auto timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
    std::chrono::system_clock::now().time_since_epoch() - std::chrono::seconds(args.period));

  if (cuInit(0) != CUDA_SUCCESS) {
    return RET_FAIL;
  }
  if (filesystem::exists(pids.PidsPath())) {
    if (pids.Refresh() != RET_SUCC) {
      return RET_FAIL;
    }
  }
  if (config.Initialize() != RET_SUCC) {
    return RET_FAIL;
  }
  if (gpu.DeviceCount() == XpuManager::INVALID_DEVICE_COUNT) {
    return RET_FAIL;
  }

  ContainerVxpuInfo info(VxpuType::VGPU);
  for (int i = 0; i < gpu.DeviceCount(); i++) {
    VxpuInfo &vgpu = info.vxpus.emplace_back(config, VxpuType::VGPU, i);
    nvmlDevice_t dev;
    int ret = FillVgpuInfo(vgpu, dev);
    if (ret != RET_SUCC) {
      return ret;
    }
    ret = FillProcInfo(vgpu, dev, gpu.PidsMap(), timestamp.count());
    if (ret != RET_SUCC) {
      return ret;
    }
  }

  if (args.format == OutputFormat::JSON) {
    fmt::print("{:j}\n", info);
  } else {
    fmt::print("{:t}\n", info);
  }
  return RET_SUCC;
}
}

#ifndef UNIT_TEST
int main(int argc, char *argv[]) {
  return xpu::CudaMonitorMain(argc, argv);
}
#endif

