/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#include <fcntl.h>
#include "log.h"
#include "memory_limiter.h"

bool MemoryLimiter::MemoryCheck(size_t requested)
{
    if (!config_.LimitMemory()) {
        return true;
    }

    size_t used;
    int ret = xpu_.MemoryUsed(used);
    if (ret) {
        log_err("get used memory failed, ret is {}", ret);
        return false;
    }

    size_t quota = config_.MemoryQuota();
    if (requested + used > quota) {
        log_err("out of memory, request {} B, used {} B, quota {} B",
            requested, used, quota);
        return false;
    }
    return true;
}

MemoryLimiter::Guard MemoryLimiter::GuardedMemoryCheck(size_t requested)
{
    FileLock lock(LockPath(), LOCK_EX);
    return {std::move(lock), MemoryCheck(requested)};
}

int MemoryLimiter::CreateFileLockBaseDir()
{
    int ret = mkdir(FILELOCK_BASE_DIR.c_str(), S_IRWXU | S_IRGRP | S_IXGRP);
    if (ret < 0 && errno != EEXIST) {
        log_err("mkdir {} failed, err is {}", FILELOCK_BASE_DIR, strerror(errno));
        return RET_FAIL;
    }
    log_info("mkdir {} succ", FILELOCK_BASE_DIR);
    return RET_SUCC;
}

int MemoryLimiter::Initialize()
{
    return CreateFileLockBaseDir();
}
