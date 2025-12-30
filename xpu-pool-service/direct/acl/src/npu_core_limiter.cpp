/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#include <fstream>
#include <unistd.h>
#include <thread>
#include "acl_resource_limiter.h"
#include "register.h"
#include "npu_core_limiter.h"
#include "log.h"
#include "common.h"

using namespace xpu;

NpuCoreLimiter::RequestGuard::RequestGuard(NpuCoreLimiter &limiter, rtContext_t ctx, rtStream_t stream)
    : limiter_(limiter)
{
    limiter_.semaphore_.Acquire();
    limiter_.streams_.ConcurrentPush(ctx, stream);
}

NpuCoreLimiter::RequestGuard::~RequestGuard()
{
    limiter_.semaphore_.Release();
}

NpuCoreLimiter::ReleaseGuard::ReleaseGuard(NpuCoreLimiter &limiter, size_t opCount)
    : limiter_(limiter), opCount_(opCount)
{
    limiter_.semaphoreBack_.Release(opCount);
}

NpuCoreLimiter::ReleaseGuard::~ReleaseGuard()
{
    opCount_ = limiter_.semaphore_.AcquireAll();
    limiter_.semaphoreBack_.Acquire(opCount_);
    limiter_.streams_.Clear();
}

int NpuCoreLimiter::Initialize()
{
    int ret = LoadVnpuIdsConfig();
    if (ret != RET_SUCC) {
        return ret;
    }
    void *shm = shm_.Init(strId_, NpuTimesliceScheduler::CONTEXT_SIZE);
    ret = sched_.Init(idx_, shm, config_.ComputingPowerQuota());
    if (ret != RET_SUCC) {
        return ret;
    }
    // must create watcher thread after load config
    return ComputingPowerWatcherInit();
}

NpuCoreLimiter::ReleaseGuard NpuCoreLimiter::ReleaseOps(size_t opCount)
{
    return ReleaseGuard(*this, opCount);
}
 
NpuCoreLimiter::RequestGuard NpuCoreLimiter::ComputingPowerLimiter(rtStream_t stream)
{
    rtContext_t ctx;
    rtError_t ret = rtCtxGetCurrent(&ctx);
    if (ret != RT_ERROR_NONE) {
        log_err("rtCtxGetCurrent failed: {%d}", ret);
    }
    return RequestGuard(*this, ctx, stream);
}
 
 void NpuCoreLimiter::ComputingPowerWatcherThread()
 {
     log_info("client(pid:{}) create acl computing power watcher thread", getpid());
     sched_.SchedulerRun();
 }
 
 int NpuCoreLimiter::ComputingPowerWatcherInit()
 {
     if (!config_.LimitComputingPower()) {
         log_info("no limit computing power, no create watcher thread");
         return RET_SUCC;
     }
     if (watcherStarted_.exchange(true)) {
         log_warn("trying to create extra watcher thread. Ignored.");
         return RET_SUCC;
     }
     try {
         watcher_ = std::thread(&NpuCoreLimiter::ComputingPowerWatcherThread, this);
         // LOG_EXC_START scenario
     } catch (const std::system_error &e) {
         log_err("create ComputingPowerWatcherThread failed with code [{}]{}", e.code().message(), e.what());
         return RET_FAIL;
     }
     // LOG_EXC_STOP
     return RET_SUCC;
 }
 
/// for read in npu-ids.config
int NpuCoreLimiter::LoadVnpuIdsConfig()
{
    std::ifstream file(VNPU_IDS_CONFIG_PATH);
    if (!file.is_open()) {
        FileOperateErrorHandler(file, VNPU_IDS_CONFIG_PATH);
         return RET_FAIL;
     }
 
     std::string line;
     if (!getline(file, line)) {
         log_err("get vpu-ids.config line failed");
         return RET_FAIL;
     }
     size_t pos = line.rfind("-");
     if (pos == std::string::npos) {
         log_err("Parse vpu-ids.config line failed");
         return RET_FAIL;
     }
     try {
         strId_ = line.substr(0, pos);
         idx_ = std::stoi(line.substr(pos + 1));
     } catch (const std::exception &e) {
         log_err("parse vpu-ids.config node idx failed");
         return RET_FAIL;
     }
     log_info("vnpu loaded: {}", line);
     return RET_SUCC;
 }
 
 NpuCoreLimiter::~NpuCoreLimiter()
 {
     JoinWatcher();
 }
 
 void NpuCoreLimiter::JoinWatcher()
 {
     watcherEnd_ = true;
     if (watcher_.joinable()) {
         try {
             watcher_.join();
         } catch (const std::system_error &e) {
             log_err("Join ComputingPowerWatcherThread failed with code [{}]{}\n", e.code().message(), e.what());
         }
     }
 }