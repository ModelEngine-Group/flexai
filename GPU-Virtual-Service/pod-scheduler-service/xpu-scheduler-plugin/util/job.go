/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

// Package util defines data structure and provide util function for xpu scheduler plugin implementation
package util

import (
	"strings"

	"k8s.io/apimachinery/pkg/types"
	"k8s.io/klog/v2"
	"volcano.sh/volcano/pkg/scheduler/api"
)

// XPUJob only xpu vcJob have.
type XPUJob struct {
	// the mapKey is taskID, not Name.
	Tasks             map[api.TaskID]*XPUTask
	SelectServers     string
	XPUTaskNum        int
	SchedulingTaskNum int
	ReqXPUName        string
	ReqXPUNum         int
	// ReqXPUInterBandwidth for minimum bandwidth rate between pods.
	// ReqXPUInterBandwidth[taskName1][taskName2]=50 means bandwidth between
	// pod in task1 and pod in task2 should larger than 50.
	// ReqXPUInterBandwidth[taskName1][taskName1]=50 means bandwidth between
	// pods in task1 should larger than 50.
	// if ReqXPUInterBandwidth[taskName1][taskName2]<=0 or not exist means they don't care bandwidth.
	ReqXPUInterBandwidth map[string]map[string]int
	// TopologyScheduleResult for topology schedule result
	TopologyScheduleResult map[api.TaskID]*TopologyScheduleXPUs
}

// TopologyScheduleXPUs for topology schedule xpu devices
type TopologyScheduleXPUs struct {
	// AllocateXPUs containerName: xpu id list
	AllocateXPUs []int
	NodeName     string
}

// GetXPUTaskNumInJob get the XPU task number in one job. for some task has no XPU.
func (xJob *XPUJob) GetXPUTaskNumInJob() int {
	if xJob == nil || !IsXPUName(xJob.ReqXPUName) {
		return 0
	}
	taskNum := 0
	for _, task := range xJob.Tasks {
		if IsXPUName(task.ReqXPUName) {
			taskNum++
		}
	}
	return taskNum
}

// GetSchedulingTaskNum get the num of scheduling task
func (xJob *XPUJob) GetSchedulingTaskNum() int {
	if xJob == nil || !IsXPUName(xJob.ReqXPUName) {
		return 0
	}
	schedulingTaskNum := 0
	for _, task := range xJob.Tasks {
		if task.NodeName == "" {
			schedulingTaskNum++
		}
	}
	return schedulingTaskNum
}

// ReferenceNameOfJob get name of job
func ReferenceNameOfJob(job *api.JobInfo) string {
	if job != nil && job.PodGroup != nil && len(job.PodGroup.OwnerReferences) > 0 {
		return job.PodGroup.OwnerReferences[0].Name
	}
	return ""
}

// UuidOfJob get uid of job
func UuidOfJob(job *api.JobInfo) types.UID {
	if job != nil && job.PodGroup != nil && len(job.PodGroup.OwnerReferences) > 0 {
		return job.PodGroup.OwnerReferences[0].UID
	}
	return ""
}

// IsSelectorMeetJob check the selectors
func IsSelectorMeetJob(jobSelectors, conf map[string]string) bool {
	for jobKey, jobValue := range jobSelectors {
		value, ok := conf[jobKey]
		if !ok {
			klog.V(LogErrorLevel).Infof("conf has no job selector key:%s.", jobKey)
			return false
		}
		if !strings.Contains(value, jobValue) {
			klog.V(LogErrorLevel).Infof("conf has no job selector value:%s.", jobValue)
			return false
		}
	}
	return true
}
