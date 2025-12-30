/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#ifndef GPU_CORE_LIMITER_H
#define GPU_CORE_LIMITER_H

#include <thread>
#include <array>
#include <atomic>
#include "common.h"
#include "resource_config.h"
#include "gpu_manager.h"

class GpuCoreLimiter {
public:
    int Initialize();
  void ComputingPowerLimiter();
  GpuCoreLimiter(ResourceConfig &config, GpuManager &gpu) : config_(config), gpu_(gpu)
  {}
  ~GpuCoreLimiter();

TESTABLE_PROTECTED:
  void ComputingPowerWatcherThread();
  int GetDelay(int idx);
  void SetDelay(int idx, int delay);
  int UpdateDelay(int idx);
  int ComputingPowerWatcherInit();
  void JoinWatcher();

  bool watcherEnd_ = false;

  class PidController {
  public:
	float kp;
	float ki;
	float kd;
	int prevDiff1;
	int prevDiff2;
	int coeffDouble;
	long CalculateDelay(int diff);
};

TESTABLE_PRIVATE:
  // 1/6s
  constexpr static auto UPDATE_PERIOD = std::chrono::milliseconds(167);
  constexpr static int MAX_DELAY = MICROSEC;
  constexpr static int BOUNDARY_LIMIT = 10;

  PidController pidController_;
  ResourceConfig &config_;
  GpuManager &gpu_;
  std::thread watcher_;
  std::array<std::atomic<int>, XpuManager::MAX_DEVICE_COUNT> delay_;
};

#endif
