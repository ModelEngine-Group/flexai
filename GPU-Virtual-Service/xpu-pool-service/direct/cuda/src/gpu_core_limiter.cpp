/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#include "gpu_core_limiter.h"
#include "log.h"

using namespace xpu;

int GpuCoreLimiter::Initialize()
{
  unsigned int upLimit = config_.ComputingPowerQuota();
  if (upLimit <= BOUNDARY_LIMIT) {
    GpuCoreLimiter::pidController_ = {10.5, 3.9, 1, 0, 0, 2};
  } else {
    GpuCoreLimiter::pidController_ = {5.5, 0.76, 1, 0, 0, 2};
  }
  return ComputingPowerWatcherInit();
}

void GpuCoreLimiter::ComputingPowerLimiter()
{
  if (!config_.LimitComputingPower()) {
    return;
  }
  int delay = GetDelay(gpu_.CurrentDevice());
  if (delay != 0) {
    std::this_thread::sleep_for(std::chrono::microseconds(delay));
  }
}

int GpuCoreLimiter::GetDelay(int idx)
{
  if (!gpu_.CheckDeviceIndex(idx)) {
    return MAX_DELAY;
  }
  return delay_[idx];
}

void GpuCoreLimiter::SetDelay(int idx, int delay)
{
  if (!gpu_.CheckDeviceIndex(idx)) {
    return;
  }
  delay_[idx] = delay;
}

long GpuCoreLimiter::PidController::CalculateDelay(int diff)
{
  long delay = lround(kp * (diff - prevDiff1) + ki * diff + kd * (diff - coeffDouble * prevDiff1 + prevDiff2));
  prevDiff2 = prevDiff1;
  prevDiff1 = diff;
  return delay;
}

int GpuCoreLimiter::UpdateDelay(int idx)
{
  unsigned int used;
  int ret = gpu_.ComputingPowerUsed(idx, used);
  if (ret != RET_SUCC) {
    return ret;
  }

  long tmpDelay;
  int diff = 0;
  unsigned int upLimit = config_.ComputingPowerQuota();
  diff = used - upLimit;
  tmpDelay = pidController_.CalculateDelay(diff) + GetDelay(idx);
  if (tmpDelay < 0) {
    tmpDelay = 0;
  } else if (tmpDelay > MAX_DELAY) {
    tmpDelay = MAX_DELAY;
  }
  SetDelay(idx, tmpDelay);
  return RET_SUCC;
}

void GpuCoreLimiter::ComputingPowerWatcherThread()
{
  while (!watcherEnd_) {
    std::this_thread::sleep_for(UPDATE_PERIOD);

    if (!config_.LimitComputingPower()) {
      continue;
    }

    int devCnt = gpu_.DeviceCount();
    if (devCnt == XpuManager::INVALID_DEVICE_COUNT) {
      continue;
    }

    for (int i = 0; i < devCnt; i++) {
      UpdateDelay(i);
    }
#ifdef UNIT_TEST
    break;
#endif
  }
}

int GpuCoreLimiter::ComputingPowerWatcherInit()
{
  if (!config_.LimitComputingPower()) {
    return RET_SUCC;
  }
  if (watcher_.joinable()) {
    return RET_SUCC;
  }
  try {
    watcher_ = std::thread(&GpuCoreLimiter::ComputingPowerWatcherThread, this);
  } catch (const std::system_error &e) {
    return RET_FAIL;
  }
  return RET_SUCC;
}

GpuCoreLimiter::~GpuCoreLimiter()
{
  JoinWatcher();
}

void GpuCoreLimiter::JoinWatcher() {
  watcherEnd_ = true;
  if (watcher_.joinable()) {
    try {
      watcher_.join();
    } catch (const std::system_error &e) {
      log_err("join computingPowerWatcherThread failed.");
    }
  }
}
