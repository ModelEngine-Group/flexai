#include "cuda_resource_limiter.h"
#include "log.h"

using namespace xpu;

CudaResourceLimiter &CudaResourceLimiter::Instance()
{
  static CudaResourceLimiter limiter;
  return limiter;
}

void CudaResourceLimiter::Initialize()
{
  std::call_once(initFlag_, [this]() {
    int ret = gpu_.InitXpu();
    if (ret == RET_SUCC) {
      ret = config_.Initialize();
    }
    if (ret == RET_SUCC) {
      ret = mem_.Initialize();
    }
    if (ret == RET_SUCC) {
      ret = core_.Initialize();
    }
    if (ret) {
      exit(EXIT_FAILURE);
    }
  });
}

void CudaResourceLimiter::ComputingPowerLimiter() {
  core_.ComputingPowerLimiter();
}

bool CudaResourceLimiter::LimitMemory() const
{
  return config_.LimitMemory();
}

size_t CudaResourceLimiter::MemoryQuota() const
{
  return config_.MemoryQuota();
}

int CudaResourceLimiter::MemoryUsed(size_t &used) {
  return gpu_.MemoryUsed(used);
}

MemoryLimiter::Guard CudaResourceLimiter::GuardedMemoryCheck(size_t requested) {
  return mem_.GuardedMemoryCheck(requested);
}

CudaResourceLimiter::CudaResourceLimiter() :
  config_(gpu_),
  mem_(config_, gpu_),
  core_(config_, gpu_)
{}
