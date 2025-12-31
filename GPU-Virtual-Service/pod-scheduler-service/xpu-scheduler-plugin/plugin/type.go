/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

// Package plugin implements xpu scheduler plugin
package plugin

import (
	"sync"

	"k8s.io/apimachinery/pkg/types"
	"volcano.sh/volcano/pkg/scheduler/api"
	"volcano.sh/volcano/pkg/scheduler/plugins/xpu-scheduler-plugin/common"
	"volcano.sh/volcano/pkg/scheduler/plugins/xpu-scheduler-plugin/util"
)

const (
	// PluginName the huawei XPU's plugin name.
	PluginName = "huaweiXPU"
)

const (
	scoreWeight              = 100
	defaultSchedulingTaskNum = -1
	scoreSplitMinSize        = 5
)

// ScheduleHandler information for the current plugin
type ScheduleHandler struct {
	XPUPlugins     map[string]XPUBuilder
	XPUDevices     map[string]map[int]*common.XPUDevice
	Jobs           map[api.JobID]*SchedulerJob
	DeleteJobInfos map[api.JobID]*api.JobInfo
	SessionID      types.UID
	Nodes          []*api.NodeInfo
	*sync.Mutex
}

// ContainerDevices description of all xpus in the container
type ContainerDevices []common.ContainerDevice

// PodDevices description of all xpus in the pod
type PodDevices []ContainerDevices

// SchedulerJob the plugin define job info
type SchedulerJob struct {
	Id            api.JobID
	ReferenceName string
	NameSpace     string
	Annotation    map[string]string
	Selector      map[string]string
	Label         map[string]string
	UnschedulableReason
	handler     XPUSchedulerPlugin
	JobReadyTag bool
	*util.XPUJob
	TopologyAllocateOnce sync.Once
}

// UnschedulableReason the message of pod pending
type UnschedulableReason struct {
	Reason map[string]string
	*sync.Mutex
}
