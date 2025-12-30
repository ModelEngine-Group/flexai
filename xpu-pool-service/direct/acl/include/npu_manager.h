/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#ifndef NPU_MANAGER_H
#define NPU_MANAGER_H

#include <array>
#include <mutex>
#include <runtime/rt.h>
#include <common.h>
#include "log.h"
#include "xpu_manager.h"

class NpuManager : public XpuManager {
public:
    int InitXpu() override;
    int MemoryUsed(size_t &used) override;
    std::string_view ConfigPath() override
    {
        return WNPU_CONFIG_PATH;
    }

    int CurrentDevice() override {
        // 在测试中发现rtGetDevice接口获取设备id时得到错误结果且错误结果比正常结果大64.
        // 推测是因为调用xook中的RT接口时，调用栈上层的栈内存都被改变（rGetDevice的功能.
        // 在返回前的最后一条指令是右移8位，所以用最后结果的最低8位可以获取到正确的值）.
        // 这里暂时把设备id的范围限制在64以内，比64大的认为是从高位移位得到.
        constexpr int32_t NPU_MAX_DEVICE_COUNT = 64;
        int32_t dev;
        rtError_t ret = rtGetDevice(&dev);
        if (ret != RT_ERROR_NONE) {
            log_err("rtGetDevice failed: {}", ret);
            return INVALID_DEVICE_IDX;
        }
        return dev % NPU_MAX_DEVICE_COUNT;
    }

    int DeviceCount() override;

    auto GetCardId(int32_t logicId) {
        std::call_once(cardMapInit_, &NpuManager::InitDeviceMap, this);
        if (logicId >= DeviceCount() || logicId < 0) {
            log_err("GetCardId wrong device: {}", logicId);
            return std::make_pair(INVALID_DEVICE_IDX, INVALID_DEVICE_IDX);
        }
        return cardMap_[logicId];
    }

TESTABLE_PRIVATE:
    int InitDeviceMap();

    int32_t deviceCnt_ = INVALID_DEVICE_COUNT;
    std::mutex mtx_;

private:
    constexpr static int MAX_PIDS = 1024;
    const std::string VNPU_CONFIG_PATH = CONFIG_BASE_DIR + "vnpu.config";
    std::once_flag cardMapInit_;
    std::array<std::pair<int, int>, MAX_DEVICE_COUNT> cardMap_;
};

#endif