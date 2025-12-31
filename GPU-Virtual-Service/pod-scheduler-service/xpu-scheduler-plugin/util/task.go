/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

// Package util defines data structure and provide util function for xpu scheduler plugin implementation
package util

import (
	"fmt"
	"reflect"
	"sync"

	"k8s.io/api/core/v1"
	"k8s.io/klog/v2"
	"volcano.sh/volcano/pkg/scheduler/api"
	"volcano.sh/volcano/pkg/scheduler/framework"
)

// XPUTask for xpu task need.
type XPUTask struct {
	Name      string
	Namespace string
	*TaskResource
	IsVXPUTask bool
	//ReqXPUIntraBandwidth for minimum bandwidth rate between pod's xpus
	ReqXPUIntraBandwidth int
	// Selector the same as job
	Selector   map[string]string
	Annotation map[string]string
	Label      map[string]string
	NodeName   string
	PodStatus  v1.PodPhase
	ScoreMap   map[string]float64
	sync.Mutex
}

// TaskResource for xpu task pod.
type TaskResource struct {
	ReqXPUName          string
	ReqXPUNum           int
	ReqXPUType          string
	ReqXPUCores         int
	ReqXPUMem           int
	ReqXPUMemPercentage int
}

// ContainerResource for xpu container
type ContainerResource TaskResource

// UpdatePodPendingReason update pod pending reason.
func (xTask *XPUTask) UpdatePodPendingReason(taskInfo *api.TaskInfo, reason string) error {
	if xTask == nil {
		klog.V(LogErrorLevel).Infof("UpdatePodPendingReason failed: %s.", ArgumentError)
		return fmt.Errorf(ArgumentError)
	}
	if xTask.Name != taskInfo.Name {
		return fmt.Errorf("XPUTask %s and TaskInfo %s does not match", xTask.Name, taskInfo.Name)
	}
	condition := v1.PodCondition{
		Type:    v1.PodScheduled,
		Status:  v1.ConditionFalse,
		Reason:  v1.PodReasonUnschedulable,
		Message: reason,
	}
	for _, tmp := range taskInfo.Pod.Status.Conditions {
		if reflect.DeepEqual(tmp, condition) {
			return nil
		}
	}
	taskInfo.Pod.Status.Conditions = append(taskInfo.Pod.Status.Conditions, condition)
	return nil
}

// GetTaskInfoByNameFromSession get corresponding api.TaskInfo object by given taskName
func GetTaskInfoByNameFromSession(ssn *framework.Session, taskName string) (*api.TaskInfo, error) {
	if ssn == nil {
		klog.V(LogErrorLevel).Infof("UpdatePodPendingReason failed: %s.", ArgumentError)
		return nil, fmt.Errorf(ArgumentError)
	}
	if len(taskName) == 0 {
		klog.V(LogErrorLevel).Infof("GetTaskInfoByNameFromSession failed: taskName is empty")
		return nil, fmt.Errorf("getTaskInfoByNameFromSession: taskName is empty")
	}
	for _, jobInfo := range ssn.Jobs {
		for _, taskInfo := range jobInfo.Tasks {
			if taskName == taskInfo.Name {
				return taskInfo, nil
			}
		}
	}
	return nil, fmt.Errorf("did not find task %s in session", taskName)
}

// ReferenceNameOfTask get pod OwnerReferences name
func ReferenceNameOfTask(task *api.TaskInfo) string {
	if task != nil && task.Pod != nil && len(task.Pod.OwnerReferences) > 0 {
		return task.Pod.OwnerReferences[0].Name
	}
	return ""
}
