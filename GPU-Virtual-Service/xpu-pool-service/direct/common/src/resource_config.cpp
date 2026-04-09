/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#include <filesystem>
#include <fstream>
#include "log.h"
#include "register.h"
#include "resource_config.h"

using namespace std;
using namespace xpu;

int ResourceConfig::Initialize()
{
    // check if client running in container
    if (!filesystem::exists(xpu_.ConfigPath())) {
        log_debug("{} no exist, client is running in host", xpu_.ConfigPath());
        return RET_SUCC;
    }
    return LoadVxpuConfig();
}

int ResourceConfig::ParseLineByConfigName(const string& line, const string& configName,
    unsigned long& value, unsigned int maxValue)
{
    string valueStr;
    string::size_type pos = line.npos;
    pos = line.rfind(configName, 0);
    if (pos == line.npos) {
        log_err("not found {}", configName);
        return RET_FAIL;
    }
    valueStr = line.substr(configName.size() + 1);  // configName:

    value = stoul(valueStr);
    if (value > maxValue) {
        log_err("parse {} failed, content {} overflow", configName, value);
        return RET_FAIL;
    }
    return RET_SUCC;
}

/*
* Format in vgpu config:
* UsedMem:xxx
* UsedCores:yyy
*/
int ResourceConfig::LoadVxpuConfig()
{
    const string configPath(xpu_.ConfigPath());
    ifstream file(configPath);
    if (!file.is_open()) {
        FileOperateErrorHandler(file, configPath);
        return RET_FAIL;
    }

    int ret;
    string line;
    unsigned long memoryValue;
    unsigned long coresValue;

    if (!getline(file, line)) {
        log_err("getting line failed while parsing UsedMem");
        return RET_FAIL;
    }
    ret = ParseLineByConfigName(line, "UsedMem", memoryValue, UINT_MAX);
    if (ret) {
        return ret;
    }
    memory_ = memoryValue * MEGABYTE;
    limitMemory_ = true;

    if (!getline(file, line)) {
        log_err("getting line failed while parsing UsedCores");
        return RET_FAIL;
    }
    ret = ParseLineByConfigName(line, "UsedCores", coresValue, PERCENT_MAX);
    if (ret) {
        return ret;
    }
    computingPower_ = static_cast<unsigned int>(coresValue);
    // if computingPower is 0, don't limit computingPower
    limitComputingPower_ = (computingPower_ != 0);

    log_info("parse {} over, the configs are as follows: ", xpu_.ConfigPath());
    log_info("limitMemory {}, limitComputingPower {}, memory {}, computingPower {}",
        limitMemory_, limitComputingPower_, memory_, computingPower_);
    return RET_SUCC;
}
