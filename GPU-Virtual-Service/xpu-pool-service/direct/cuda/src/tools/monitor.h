#ifndef MONITOR_H
#define MONITOR_H

#include "tools/monitor_base.h"
#include "gpu_manager.h"

namespace xpu {
int FillProcMem(VxpuInfo &info, PidManager &pids, nvmlDevice_t dev);

int FillProcCore(VxpuInfo &info, PidManager &pids, nvmlDevice_t dev, size_t timestamp);

int FillVgpuInfo(VxpuInfo &info, nvmlDevice_t &dev);

int FillProcInfo(VxpuInfo &info, nvmlDevice_t dev, PidManager &pids, size_t timestamp);

int CudaMonitorMain(int argc, char *argv[]);
}

#endif
