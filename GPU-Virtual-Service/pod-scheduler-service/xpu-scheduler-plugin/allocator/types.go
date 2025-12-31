/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

package allocator

import (
	"errors"

	"volcano.sh/volcano/pkg/scheduler/api"
	"volcano.sh/volcano/pkg/scheduler/plugins/xpu-scheduler-plugin/common"
)

var (
	ErrCannotAllocation = errors.New("cannot allocate")
	numa                bool
)

type NodeResource struct {
	NodeName     string
	Topology     [][]int
	UnuseDevices map[int]*common.XPUDevice
	CardType     []string
}

type PodCardRequest struct {
	TaskId         api.TaskID
	TaskName       string
	NumberOfCard   int
	IntraBandWidth int
	CardType       string
}

type PodAllocation struct {
	TaskId    api.TaskID
	NodeName  string
	DeviceIds []int
}
