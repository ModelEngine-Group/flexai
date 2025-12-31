/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

package xpu

import (
	"volcano.sh/volcano/pkg/scheduler/plugins/xpu-scheduler-plugin/plugin"
	"volcano.sh/volcano/pkg/scheduler/plugins/xpu-scheduler-plugin/util"
)

var (
	gpuPlugin = &plugin.SchedulerPlugin{
		PluginName:                 util.GPUPluginName,
		VxpuName:                   util.VGPUName,
		VxpuType:                   util.VGPUType,
		VxpuCore:                   util.VGPUCore,
		VxpuMemory:                 util.VGPUMemory,
		Config:                     Config,
		NodeXPURegisterAnno:        util.NodeGPURegisterAnnotation,
		AssignedXPUsToAllocateAnno: util.AssignedGPUsToAllocateAnnotations,
		AssignedXPUsToNodeAnno:     util.AssignedGPUsToNodeAnnotations,
		AssignedXPUsToPodAnno:      util.AssignedGPUsToPodAnnotations,
		NodeXPUTopologyAnno:        util.NodeGPUTopologyAnnotation,
		NodeXPUHandshakeAnno:       util.NodeGPUHandshakeAnnotation,
	}

	npuPlugin = &plugin.SchedulerPlugin{
		PluginName:                 util.NPUPluginName,
		VxpuName:                   util.VNPUName,
		VxpuType:                   util.VNPUType,
		VxpuCore:                   util.VNPUCore,
		VxpuMemory:                 util.VNPUMemory,
		Config:                     Config,
		NodeXPURegisterAnno:        util.NodeNPURegisterAnnotation,
		AssignedXPUsToAllocateAnno: util.AssignedNPUsToAllocateAnnotations,
		AssignedXPUsToNodeAnno:     util.AssignedNPUsToNodeAnnotations,
		AssignedXPUsToPodAnno:      util.AssignedNPUsToPodAnnotations,
		NodeXPUTopologyAnno:        util.NodeNPUTopologyAnnotation,
		NodeXPUHandshakeAnno:       util.NodeNPUHandshakeAnnotation,
	}

	Config = &plugin.CommonConfig{}
)
