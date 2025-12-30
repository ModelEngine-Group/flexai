/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */
 
#include <fcntl.h>
#include <sys/mman.h>
#include "shm.h"
#include "log.h"

Shm::~Shm()
{
    if (addr_ != nullptr) {
        munmap(addr_, size_);
        addr_ = nullptr;
    }
    if (fd_ != -1){
        close(fd_);
        fd_ = -1;
    }
}

void *Shm::Init(const std::string &dieID, size_t size)
{
    size_ = size;
    fd_ = shm_open(dieID.c_str(), O_CREAT | O_RDWR, S_IWUSR | S_IRUSR);
    if (ftruncate(fd_, size)) {
        return nullptr;
    }
    addr_ = mmap(nullptr, size, PROT_READ | PORT_WRITE, MAP_SHARED, fd_, 0);
    if (addr_ == MAP_FAILED) {
        return nullptr;
    }
    return addr_;
}