/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#ifndef RESOURCE_CONFIG_H
#define RESOURCE_CONFIG_H

#include <cstddef>
#include <string>
#include "common.h"
#include "xpu_manager.h"

class ResourceConfig {
public:
    ResourceConfig(XpuManager &xpu) : xpu_(xpu)
    {}
    int Initialize();
    int LoadVxpuConfig();

    size_t MemoryQuota() const
    {
        return memory_;
    }

    bool LimitMemory() const
    {
        return limitMemory_;
    }

    bool LimitComputingPower() const
    {
        return limitComputingPower_;
    }

    unsigned int ComputingPowerQuota() const
    {
        return computingPower_;
    }

TESTABLE_PRIVATE:
    int ParseLineByConfigName(const std::string& line, const std::string& configName,
        unsigned long& value, unsigned int maxValue);

    XpuManager &xpu_;
    size_t memory_ = 0;         // Bytes
    unsigned int computingPower_ = 0; // %
    bool limitMemory_ = false;
    bool limitComputingPower_ = false;
};

#endif