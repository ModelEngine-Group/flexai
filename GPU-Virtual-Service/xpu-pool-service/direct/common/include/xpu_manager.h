/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#ifndef XPU_MANAGER_H
#define XPU_MANAGER_H

#include <string>

class XpuManager {
public:
    virtual int InitXpu() = 0;
    // return device count if possible, else return INVALID_DEVICE_COUNT (-1) if not initialized
    virtual int DeviceCount() = 0;
    // return current device index if possible, else return INVALID_DEVICE_IDX (-1)
    virtual int CurrentDevice() = 0;
    virtual int MemoryUsed(size_t &used) = 0;
    virtual std::string_view ConfigPath() = 0;
    bool CheckDeviceIndex(int idx)
    {
        return idx >= 0 && idx < DeviceCount();
    }

    const std::string CONFIG_BASE_DIR = "/etc/xpu/";
    constexpr static int MAX_DEVICE_COUNT = 16;
    constexpr static int INVALID_DEVICE_COUNT = -1;
    constexpr static int INVALID_DEVICE_IDX = -1;
};

#endif