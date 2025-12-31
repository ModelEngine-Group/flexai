/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#ifndef MONITOR_H
#define MONITOR_H

#include "tools/monitor_base.h"

namespace xpu {
int FillVnpuInfo(VxpuInfo &info, uint32_t cardId, uint32_t deviceId);
int FillProcInfo(VxpuInfo &info, uint32_t cardId, uint32_t deviceId);
int AclMonitorMain(int argc, char *argv[]);
} // namespace xpu

#endif