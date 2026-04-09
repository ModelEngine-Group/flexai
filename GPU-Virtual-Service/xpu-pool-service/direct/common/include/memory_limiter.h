/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#ifndef MEMORY_LIMITER_H
#define MEMORY_LIMITER_H

#include <cstddef>
#include <string>
#include "common.h"
#include "file_lock.h"
#include "xpu_manager.h"
#include "resource_config.h"

class MemoryLimiter {
public:
    struct Guard {
        FileLock lock;
        bool enough;
        bool Error()
        {
            return !lock.Held();
        }
    };

    Guard GuardedMemoryCheck(size_t requested);

    MemoryLimiter(ResourceConfig &config, XpuManager &xpu) : config_(config), xpu_(xpu)
    {}
    int Initialize();
TESTABLE_PROTECTED:
    bool MemoryCheck(size_t requested);
    const std::string_view LockPath()
    {
        return MEMCTL_LOCK_PATH;
    }

TESTABLE_PRIVATE:
    int CreateFileLockBaseDir();

    const std::string FILELOCK_BASE_DIR = "/run/xpu/";
    const std::string MEMCTL_LOCK_PATH = FILELOCK_BASE_DIR + "memctl.lock";
    ResourceConfig &config_;
    XpuManager &xpu_;
};

#endif