/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#ifndef FILE_LOCK_H
#define FILE_LOCK_H

#include <string_view>

class FileLock {
public:
    FileLock(const std::string_view path, int operation);
    FileLock(const FileLock& other) = delete;
    FileLock& operator=(const FileLock& other) = delete;
    FileLock(FileLock&& other) noexcept: fd_(other.fd_), held_(other.held_)
    {
        other.fd_ = INVALID_FD;
        other.held_ = false;
    }
    FileLock& operator=(FileLock&& other) noexcept
    {
        FileLock temp(std::move(other));
        std::swap(fd_, temp.fd_);
        std::swap(held_, temp.held_);
        return *this;
    }
    ~FileLock();

    bool Aquire(int operation);
    bool Release();
    bool Held() const
    {
        return held_;
    }

private:
    constexpr static int INVALID_FD = -1;
    int fd_;
    bool held_;
};

#endif
