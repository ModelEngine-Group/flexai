#ifndef CUDA_RESOURCE_LIMITER_H
#define CUDA_RESOURCE_LIMITER_H

#include "gpu_manager.h"
#include "memory_limiter.h"
#include "resource_config.h"
#include "gpu_core_limiter.h"

class CudaResourceLimiter
{
public:
  static CudaResourceLimiter &Instance();

  void Initialize();
  void ComputingPowerLimiter();
  bool LimitMemory() const;
  size_t MemoryQuota() const;
  int MemoryUsed(size_t &used);
  MemoryLimiter::Guard GuardedMemoryCheck(size_t requested);

TESTABLE_PRIVATE:
  CudaResourceLimiter();

  std::once_flag initFlag_;
  GpuManager gpu_;
  ResourceConfig config_;
  MemoryLimiter mem_;
  GpuCoreLimiter core_;
};

#endif
