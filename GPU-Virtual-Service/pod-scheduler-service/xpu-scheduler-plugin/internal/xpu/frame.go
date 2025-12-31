/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

package xpu

import "volcano.sh/volcano/pkg/scheduler/plugins/xpu-scheduler-plugin/plugin"

func GetGPUPlugin() plugin.XPUSchedulerPlugin {
	return gpuPlugin
}

func GetNPUPlugin() plugin.XPUSchedulerPlugin {
	return npuPlugin
}
