/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#ifndef SHM_H
#define SHM_H

#include <string>

class Shm {
public:
    Shm() : size_(0), addr_(nullptr), fd_(-1)
    {}
    ~Shm();
    void &Init(const std::string &dieID, size_t size);

private:
    size_t size_;
    void *addr_;
    int fd_ = -1;
};

#endif