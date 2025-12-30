/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#include <string>
#include <sys/file.h>
#include "log.h"
#include "common.h"
#include "file_lock.h"

FileLock::FileLock(const std::string_view path, int operation) : held_(false)
{
    fd_ = open(path.data(), O_CREAT | O_RDONLY, 0); // the perm of lock file is 0600
    if (fd_ == -1) {
        return;
    }
    Aquire(operation);
}

bool FileLock::Aquire(int operation)
{
    /*
    * (1) The flock can block when anyone else holding the lock;
    * (2) The flock can be released either by calling the LOCK_UN parameter or
    *     by closing fd (the first parameter in flock), which means that flock
    *     is automatically released when the process is exited.
    */
    int ret = flock(fd_, operation);
    if (ret) {
        return false;
    }
    held_ = true;
    return true;
}

bool FileLock::Release()
{
    int ret = flock(fd_, LOCK_UN);
    if (ret) {
        return false;
    }
    held_ = false;
    return true;
}

FileLock::~FileLock()
{
    if (fd_ < 0) {
        return;
    }
    if (held_) {
        Release();
    }
    if (close(fd_) == -1) {
        return;
    }
}
