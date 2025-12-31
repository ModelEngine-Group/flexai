/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

package plugin

import (
	"fmt"

	"volcano.sh/volcano/pkg/scheduler/api"
	"volcano.sh/volcano/pkg/scheduler/plugins/xpu-scheduler-plugin/util"
)

func (sh *ScheduleHandler) initXPUDevicesOfNode(sJob *SchedulerJob, node *api.NodeInfo) {
	xpuDevices := sJob.handler.GetXPUDevicesFromNode(node)
	sh.Lock()
	sh.XPUDevices[node.Name] = xpuDevices
	sh.Unlock()
}

func (sh *ScheduleHandler) NodePredicate(task *api.TaskInfo, node *api.NodeInfo) error {
	if sh == nil || task == nil || node == nil {
		return fmt.Errorf("invalid input")
	}
	sJob, ok := sh.Jobs[task.Job]
	if !ok {
		return nil
	}
	if !util.IsXPUName(sJob.ReqXPUName) || !IsXPUTask(sJob, task) {
		return nil
	}
	if err := sJob.preCheckNodePredicate(task, node); err != nil {
		return err
	}

	err := sJob.handler.NodePredicateForTask(sJob, task, node, sh)
	if err != nil {
		return err
	}
	return nil
}
