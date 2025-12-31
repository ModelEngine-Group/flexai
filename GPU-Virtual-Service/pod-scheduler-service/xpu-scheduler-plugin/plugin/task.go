/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

// Package plugin implements xpu scheduler plugin
package plugin

import (
	"k8s.io/klog/v2"
	"volcano.sh/volcano/pkg/scheduler/api"
	"volcano.sh/volcano/pkg/scheduler/framework"
	"volcano.sh/volcano/pkg/scheduler/plugins/xpu-scheduler-plugin/util"
)

// IsXPUTask to judge the task either is XPU task or not.
func IsXPUTask(sJob *SchedulerJob, task *api.TaskInfo) bool {
	if _, ok := sJob.Tasks[task.UID]; !ok {
		return false
	}
	if util.IsXPUName(sJob.Tasks[task.UID].ReqXPUName) {
		return true
	}
	return false
}

// IsTaskNeedXPUAllocated to judge the task is static cut. true is dynamic cut.
func (sh ScheduleHandler) IsTaskNeedXPUAllocated(sJob *SchedulerJob, task *api.TaskInfo) bool {
	if !IsXPUTask(sJob, task) {
		klog.V(util.LogDebugLevel).Infof("IsTaskNeedXPUAllocated %s not xpu task.", task.Name)
		return false
	}
	return true
}

// XPUAllocateFunc Allocate xpu and called by volcano frame
func (sh ScheduleHandler) XPUAllocateFunc(task *api.TaskInfo, ssn *framework.Session) {
	if task == nil {
		klog.V(util.LogErrorLevel).Infof("XPUAllocateFunc %s.", util.ArgumentError)
		return
	}
	sJob, ok := sh.Jobs[task.Job]
	if !ok {
		klog.V(util.LogDebugLevel).Infof("XPUAllocateFunc %s not req npu.", task.Name)
		return
	}
	if !sh.IsTaskNeedXPUAllocated(sJob, task) {
		klog.V(util.LogDebugLevel).Infof("XPUAllocateFunc %s no need to set pod annotation.", task.Name)
		return
	}
	if !sJob.JobReadyTag {
		klog.V(util.LogDebugLevel).Infof("XPUAllocateFunc %s not allow allocate npu.", task.Name)
		return
	}
	nodeName := task.NodeName
	node, found := ssn.Nodes[nodeName]
	if !found {
		klog.V(util.LogWarningLevel).Infof("%s XPUAllocateFunc %s not exist.", PluginName, nodeName)
		return
	}

	sh.GetAllocatableXPUDeviceOnNodes(sJob)
	err := sJob.handler.Allocate(sJob, task, node, sh.getXPUDevicesOfNode(nodeName))
	if err != nil {
		klog.V(util.LogErrorLevel).Infof("XPUAllocateFunc allocate failed: %s.", util.ArgumentError)
	}
}

// XPUDeallocateFunc Free assigned xpu, if allocate failed by volcano frame.
func (sh *ScheduleHandler) XPUDeallocateFunc(task *api.TaskInfo) {
	if sh == nil || task == nil {
		klog.V(util.LogErrorLevel).Infof("XPUDeallocateFunc failed %s.", util.ArgumentError)
		return
	}
}
