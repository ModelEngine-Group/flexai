/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

 #include <fmt/ranges.h>
 #include <vector>
 #include <getopt.h>
 #include <dcmi_interface_api.h>
 #include "npu_manager.h"
 #include "resource_config.h"
 #include "tools/monitor_base.h"
 #include "log.h"
 
 using namespace std;

 namespace xpu {
 
 int FillVnpuInfo(VxpuInfo &info, uint32_t cardId, uint32_t deviceId)
 {
     if (info.memoryQuota == 0) {
         dcmi_get_memory_info_stru memInfo;
         int ret = dcmi_get_device_memory_info_v3(cardId, deviceId, &memInfo);
         if (ret == DCMI_OK) {
             info.memoryQuota = memInfo.memory_size * MEGABYTE;
         } else {
             info.memoryQuota = 0;
             log_err("failed to get memory size * MEAGBYTE;");
             return RET_FAIL;
         }
     }
     int ret = dcmi_get_device_utilization_rate(cardId, deviceId, DCMI_UTILIZATION_RATE_AICORE, &info.core);
     if (ret != DCMI_OK) {
         log_err("failed to get core utilization for card {}, device {}", cardId, deviceId);
         return RET_FAIL;
     }
     return RET_SUCC;
 }
 
 int FillProcInfo(VxpuInfo &info, uint32_t cardId, uint32_t deviceId)
 {
     dcmi_proc_mem_info procInfo[MAX_PIDS] = {};
     int procNum = MAX_PIDS;
     int ret = dcmi_get_device_resource_info(cardId, deviceId, procInfo, &procNum);
     if (ret != DCMI_OK) {
         log_err("failed to get process memory usage for card {}, device {}", cardId, deviceId);
         return RET_FAIL;
     }
     info.memory = 0;
     for (int i = 0; i < procNum; i++) {
         info.processes[procInfo[i].proc_id] = {info.core, procInfo[i].proc_mem_usage};
         info.memory += procInfo[i].proc_mem_usage;
     }
     return RET_SUCC;
 }
 
 int AclMonitorMain(int argc, char *argv[])
 {
    LogInit("npu-monitor", "0");
    Args args;
    NpuManager npu;
    ResourceConfig config(npu);
    if (ParseArgs(args, argc, argv) != RET_SUCC) {
         return RET_FAIL;
     }
     if (npu.InitXpu() != RET_SUCC) {
         return RET_FAIL;
     }
     if (config.Initialize() != RET_SUCC) {
         return RET_FAIL;
     }
     if (npu.DeviceCount() == XpuManager::INVALID_DEVICE_COUNT) {
         return RET_FAIL;
     }
 
     ContainerVxpuInfo info(VxpuType::VNPU);
     // Get data
    for (int i = 0; i < npu.DeviceCount(); i++) {
        Vxpuinfo &vnpu = info.vxpus.emplace_back(config, VxpuType::VNPU, i);
        auto cardId = npu.GetCardId(i);
        int ret = FillVnpuInfo(vnpu, cardId.first, cardId.second);
        if (ret != RET_SUCC) {
            return ret;
        }
        ret = FillProcInfo(vnpu, cardId.first, cardId.second);
        if (ret != RET_SUCC) {
            return ret;
        }
    }
 
     // output result
     if (args.format == OutputFormat::JSON) {
         fmt::print("{:j}\n", info);
     } else {
         fmt::print("{:t}\n", info);
     }
     return RET_SUCC;
 }
 
 } // namespace xpu
 
 #ifdef UNIT_TEST
 int main(int argc, char *argv[])
 {
     return xpu::AclMonitorMain(argc, argv);
 }
 #endif