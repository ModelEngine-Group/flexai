/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

// Package plugin implements xpu scheduler plugin
package plugin

import (
	"strconv"
	"strings"

	"k8s.io/api/core/v1"
	"k8s.io/klog/v2"
	"volcano.sh/volcano/pkg/scheduler/plugins/xpu-scheduler-plugin/common"
	"volcano.sh/volcano/pkg/scheduler/plugins/xpu-scheduler-plugin/util"
)

// DecodeNodeXPUTopology for decode nodes's xpu topology info
func DecodeNodeXPUTopology(s string) ([][]int, bool) {
	topoList := strings.Split(s, util.Semicolon)
	sqrtNum := len(topoList)
	if sqrtNum < util.TopologyMinNum || sqrtNum > util.TopologyMaxNum {
		klog.V(util.LogWarningLevel).Infof("DecodeXPUTopology failed, topology str: %s, device num is out of range, "+
			"device num: %d, range: %d-%d.", s, sqrtNum, util.TopologyMinNum, util.TopologyMaxNum)
		return nil, false
	}

	var res [][]int
	for _, v := range topoList {
		tmp := strings.Split(v, util.Comma)
		if len(tmp) != sqrtNum {
			klog.V(util.LogWarningLevel).Infof("DecodeXPUTopology failed, topology str is invalid: %s", s)
			return nil, false
		}
		var bandwidthList []int
		for _, x := range tmp {
			bandwidth, err := strconv.Atoi(x)
			if err != nil {
				klog.V(util.LogWarningLevel).Infof("DecodeXPUTopology failed, topology str: %s", s)
				return nil, false
			}
			bandwidthList = append(bandwidthList, bandwidth)
		}
		res = append(res, bandwidthList)
	}
	return res, true
}

// GetXPUTopologyInterBandwidth get xpu topology interBandwidth from podgroup annotations if configured
func GetXPUTopologyInterBandwidth(sJob *SchedulerJob) map[string]map[string]int {
	taskStr, ok := sJob.Annotation[util.XPUTopologyTaskListAnnotation]
	if !ok {
		return nil
	}
	taskList := strings.Split(taskStr, util.Comma)
	if interBandwidth, ok := sJob.Annotation[util.XPUTopologyInterBandwidthAnnotation]; ok {
		matrix := strings.Split(interBandwidth, util.Semicolon)
		if len(taskList) != len(matrix) {
			klog.V(util.LogErrorLevel).Infof("job %s interBandwidth length is different from task list",
				sJob.ReferenceName)
			return nil
		}
		interBandwidthMatrix, err := util.ConvertMatrix2Map(matrix, taskList)
		if err != nil {
			klog.V(util.LogErrorLevel).Infof("job %s get interbandwidth failed, err: %v",
				sJob.ReferenceName, err.Error())
			return nil
		}
		klog.V(util.LogDebugLevel).Infof("job %s XPUTopologyInterBandwidth: %+v",
			sJob.ReferenceName, interBandwidthMatrix)
		return interBandwidthMatrix
	}
	return nil
}

// GetXPUTopologyIntraBandwidth get xpu topology intraBandwidth if configured
func GetXPUTopologyIntraBandwidth(pod *v1.Pod) int {
	for _, c := range pod.Spec.Containers {
		resourceNum, ok := c.Resources.Limits[v1.ResourceName(util.XPUTopologyIntraBandwidthAnnotation)]
		if ok {
			return int(resourceNum.Value())
		}
	}
	return 0
}

// ScheduleXPUTopologyForTask schedule xpu topology for task
func ScheduleXPUTopologyForTask(reqXPUNum int, reqXPUType string, intraBandwidth int, xpuTopology [][]int,
	unUseDevices map[int]*common.XPUDevice) [][]string {
	return nil
}
