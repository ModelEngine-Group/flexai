/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#ifndef GPU_MANAGER_H
#define GPU_MANAGER_H

#include <array>
#include <mutex>
#include <unordered_map>
#include <cuda.h>
#include <nvml_compat.h>
#include "pid_manager.h"
#include "common.h"
#include "xpu_manager.h"

class GpuManager : public XpuManager {
public:
  GpuManager() : pid_(CONFIG_BASE_DIR)
  {}
  int InitXpu() override;
  int ComputingPowerUsed(int idx, unsigned int &used);
  int MemoryUsed(size_t &used) override;
  int CurrentDevice() override;
  int DeviceCount() override {
    std::call_once(handleMapInit_, &GpuManager::InitDeviceMap, this);
    return devCnt_;
  }
  std::string_view ConfigPath() override {
    return VGPU_CONFIG_PATH;
  }
  PidManager &PidsMap() {
    return pid_;
  }

  constexpr static CUdevice INVALID_CUDEVICE_HANDLE = -1;
  constexpr static nvmlDevice_t INVALID_NVML_HANDLE = nullptr;

TESTABLE_PRIVATE:
  int InitDeviceMap();

  nvmlDevice_t GetNvmlHandle(int idx);
  int GetCudaDeviceId(CUdevice dev);
  nvmlDevice_t GetCurrNvmlHandle() {
	return GetNvmlHandle(CurrentDevice());
}

private:
  constexpr static int MAX_PIDS = 1024;
  constexpr static int SHORT_PROC_UTIL_PERIOD = 1;
  constexpr static int LONG_PROC_UTIL_PERIOD = 10;
  const std::string VGPU_CONFIG_PATH = CONFIG_BASE_DIR + "vgpu.config";

  PidManager pid_;
  std::once_flag initFlag_;
  std::once_flag handleMapInit_;
  int devCnt_ = INVALID_DEVICE_COUNT;
  std::unordered_map<CUdevice, int> cuDevice_;
  std::array<nvmlDevice_t, MAX_DEVICE_COUNT> nvmlDevice_;
};

#endif
