/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#include <future>
#include "device_interface_api.h"
#include "npu_manager.h"
#include "log.h"

using namespace xpu;

int NpuManager::InitXpu()
{
    int ret = rtGetDeviceCount(&deviceCnt_);
    if (ret != RT_ERROR_NONE) {
        log_err("rtGetDeviceCount failed, ret is {}", ret);
        deviceCnt_ = INVALID_DEVICE_COUNT;
        return RET_FAIL;
    }
    if (deviceCnt_ == 0 || deviceCnt_ > MAX_DEVICE_COUNT || deviceCnt_ == INVALID_DEVICE_COUNT) {
        log_err("rtGetDeviceCount failed count is {}", deviceCnt_);
        deviceCnt_ = INVALID_DEVICE_COUNT;
        return RET_FAIL;
    }
    ret = dcmi_init();
    if (ret != DCMI_OK) {
        log_err("dcmi_init failed, ret is {}", ret);
        return RET_FAIL;
    } else {
        return RET_SUCC;
    }
}

int NpuManager::MemoryUsed(size_t &used)
{
    dcmi_proc_mem_info procInfo[PXD_MAX] = {0};
    std::call_once(cardMapInit_, &NpuManager::InitDeviceMap, this);
    int deviceId = CurrentDevice();
    if (deviceId == INVALID_DEVICE_IDX) {
        return DCMI_ERR_CODE_INVALID_DEVICE_ID;
    }
    auto cardId = GetCardId(deviceId);

    ret = dcmi_get_device_resource_info(cardId.first, cardId.second, procInfo, &procNum_).get();
    if (ret != DCMI_OK) {
        log_err("dcmi get device resource info failed, ret is {}", ret);
        return ret;
    }
    if (procNum_ > MAX_PROCS) {
        log_err("dcmi get device resource too many processes, count is {}", procNum_);
        return DCMI_ERR_CODE_INNER_ERROR;
    }
    used = 0;
    for (int i = 0; i < procNum_; i++) {
        used += procInfo[i].proc_mem_usage;
    }
    log_debug("sdk get mem used as {}", used);
    return DCMI_OK;
}

int NpuManager::DeviceCount()
{
    return deviceCnt_;
}

int NpuManager::InitDeviceMap()
{
    int cnt = DeviceCount();
    if (cnt == INVALID_DEVICE_COUNT) {
        log_err("device count is invalid");
        return RET_FAIL;
    }
    for (int i = 0; i < cnt; i++) {
        auto cardId = GetCardId(i);
        int ret = dcmi_get_card_id_sd(cardId.first, cardId.second, 3);
        if (ret != DCMI_OK) {
            log_err("dcmi get device sd failed, ret is {}", ret);
            return RET_FAIL;
        }
    }
    return RET_SUCC;
}