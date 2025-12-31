/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#ifndef ACL_RESOURCE_LIMITER_H
#define ACL_RESOURCE_LIMITER_H

#include "resource_config.h"
#include "memory_limiter.h"
#include "npu_core_limiter.h"
#include "npu_manager.h"

class AclResourceLimiter {
public:
    static AclResourceLimiter &Instance();

    void Initialize();
    NpuCoreLimiter::RequestGuard ComputingPowerLimiter(rtStream_t stm);
    NpuCoreLimiter::ReleaseGuard ReleaseOps(size_t &opCount);
    MemoryLimiter::Guard GuardedMemoryCheck(size_t requested);

TESTABLE_PRIVATE:
    AclResourceLimiter();

    NpuManager npu_;
    ResourceConfig config_;
    MemoryLimiter mem_;
    NpuCoreLimiter core_;

private:
    std::once_flag initFlag_;
};

#endif