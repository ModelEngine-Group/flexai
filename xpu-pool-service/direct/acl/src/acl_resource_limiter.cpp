/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#include <future>
#include "acl_resource_limiter.h"

using namespace xpu;

AclResourceLimiter &AclResourceLimiter::Instance()
{
    static AclResourceLimiter limiter;
    return limiter;
}

void AclResourceLimiter::Initialize()
{
    std::call_once(initFlag_, [this]() {
        LogInit("xpu_direct", "0");
        int ret = npu_.InitXpu();
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
            log_err("Initialize failed");
            exit(EXIT_FAILURE);
        }
    });
}

NpuCoreLimiter::RequestGuard AclResourceLimiter::ComputingPowerLimiter(rtStream_t stm)
{
    return core_.ComputingPowerLimiter(stm);
}

NpuCoreLimiter::ReleaseGuard AclResourceLimiter::ReleaseOps(size_t &opCount)
{
    return core_.ReleaseOps(opCount);
}

MemoryLimiter::Guard AclResourceLimiter::GuardedMemoryCheck(size_t requested)
{
    return mem_.GuardedMemoryCheck(requested);
}

AclResourceLimiter::AclResourceLimiter() : config_(npu_), mem_(config_, npu_), core_(config_, npu_)
{}