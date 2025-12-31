/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

package plugin

import (
	"errors"
	"fmt"
	"strings"

	"k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"volcano.sh/volcano/pkg/scheduler/api"
	"volcano.sh/volcano/pkg/scheduler/framework"
	"volcano.sh/volcano/pkg/scheduler/plugins/xpu-scheduler-plugin/common"
	"volcano.sh/volcano/pkg/scheduler/plugins/xpu-scheduler-plugin/util"
)

func (sh *ScheduleHandler) checkSession(ssn *framework.Session) error {
	if sh == nil || ssn == nil {
		return errors.New("session is nil")
	}
	return nil
}

func (sh *ScheduleHandler) RegisterXPUScheduler(name string, xb XPUBuilder) {
	if sh == nil || xb == nil {
		return
	}
	if _, ok := sh.XPUPlugins[name]; ok {
		return
	}

	sh.XPUPlugins[name] = xb
}

func (sh *ScheduleHandler) UnRegisterXPUScheduler(name string) error {
	if sh == nil {
		return errors.New(util.ArgumentError)
	}
	if _, ok := sh.XPUPlugins[name]; ok {
		sh.XPUPlugins[name] = nil
		delete(sh.XPUPlugins, name)
	}
	return nil
}

func (sh *ScheduleHandler) IsPluginRegistered(name string) bool {
	if sh == nil {
		return false
	}
	for k := range sh.XPUPlugins {
		if k == name {
			return true
		}
	}
	return false
}

func (sh *ScheduleHandler) InitXPUSession(ssn *framework.Session) error {
	if sh == nil || ssn == nil {
		return errors.New(util.ArgumentError)
	}
	if err := sh.checkSession(ssn); err != nil {
		return err
	}

	sh.InitJobsFromSession(ssn)
	sh.InitDeleteJobInfos()
	sh.SessionID = ssn.UID
	sh.Nodes = ssn.NodeList
	return nil
}

func (sh *ScheduleHandler) InitDeleteJobInfos() {
	if sh == nil {
		return
	}
	sh.DeleteJobInfos = map[api.JobID]*api.JobInfo{}
}

func (sh *ScheduleHandler) getXPUDevicesOfNode(nodeName string) map[int]*common.XPUDevice {
	sh.Lock()
	xpuDevices, ok := sh.XPUDevices[nodeName]
	sh.Unlock()
	if !ok {
		xpuDevices = map[int]*common.XPUDevice{}
	}
	return xpuDevices
}

func getTaskScheduledResult(task *api.TaskInfo, sJob *SchedulerJob, scoreMap map[string]float64) bool {
	if task == nil || sJob == nil || scoreMap == nil {
		return false
	}
	if _, exist := sJob.TopologyScheduleResult[task.UID]; !exist {
		return false
	}
	if _, exist := sJob.Tasks[task.UID]; !exist {
		return false
	}

	nodeName := sJob.TopologyScheduleResult[task.UID].NodeName
	sJob.Tasks[task.UID].NodeName = nodeName
	scoreMap[nodeName] = scoreWeight * scoreWeight
	return true
}

func (sh *ScheduleHandler) BatchNodeOrderFn(task *api.TaskInfo, nodes []*api.NodeInfo) (map[string]float64, error) {
	if sh == nil || task == nil || len(nodes) == 0 {
		return nil, errors.New(util.ArgumentError)
	}

	scoreMap := initScoreMap(nodes)
	sJob, ok := sh.Jobs[task.Job]
	if !ok {
		return scoreMap, nil
	}

	xpuTask, ok := sJob.Tasks[task.UID]
	if !ok {
		return make(map[string]float64), nil
	}
	return xpuTask.ScoreMap, nil
}

func (sh *ScheduleHandler) GetAllocatableXPUDeviceOnNodes(sJob *SchedulerJob) map[string][]*common.XPUDevice {
	inUseDevicesOfTopology := GetXPUDevicesFromTopologyScheduleResult(sh.Jobs)
	unUseXPUDevicesOfNodes := make(map[string][]*common.XPUDevice)
	for _, v := range sh.Nodes {
		sh.initXPUDevicesOfNode(sJob, v)
		xpuDevices := sh.getXPUDevicesOfNode(v.Name)
		sh.Lock()
		UpdateXPUDevicesFromTopologyResults(xpuDevices, inUseDevicesOfTopology[v.Name])
		sh.Unlock()
		unUseXPUDevices := GetXPUDevicesNotInUse(xpuDevices, inUseDevicesOfTopology, v.Name)
		unUseXPUDevicesOfNodes[v.Name] = unUseXPUDevices
	}
	return unUseXPUDevicesOfNodes
}

func (sh *ScheduleHandler) InitJobsFromSession(ssn *framework.Session) {
	if sh == nil || ssn == nil {
		return
	}
	sh.Jobs = make(map[api.JobID]*SchedulerJob, util.MapInitNum)
	for jobID, jobInfo := range ssn.Jobs {
		sJob := SchedulerJob{}
		if err := sJob.Init(jobInfo, sh); err != nil {
			continue
		}
		sh.Jobs[jobID] = &sJob
	}
	return
}

func (sh *ScheduleHandler) JobValid(obj interface{}) *api.ValidateResult {
	if sh == nil {
		return &api.ValidateResult{Pass: false, Reason: util.ObjectNilError,
			Message: fmt.Sprintf("validJobFn [%#v] failed:%s", obj, util.ObjectNilError)}
	}
	job, ok := obj.(*api.JobInfo)
	if !ok {
		return &api.ValidateResult{
			Pass:    false,
			Reason:  "Job convert failed",
			Message: fmt.Sprintf("Failed to convert <%v> to *JobInfo", obj),
		}
	}
	if !IsJobInitial(job) {
		return &api.ValidateResult{
			Pass:    false,
			Reason:  "Job is not in initial state",
			Message: fmt.Sprintf("Job <%s/%s> is not in initial state", job.Namespace, job.Name),
		}
	}
	vcJob, ok := sh.Jobs[job.UID]
	if !ok {
		return nil
	}

	result := vcJob.ValidJobFn()
	if result != nil {
		_ = sh.SetJobPendingReason(job, result.Message)
		return result
	}
	return nil
}

func updatePodPendingReason(task *api.TaskInfo, tmpReason string) {
	condition := v1.PodCondition{
		Type:    v1.PodScheduled,
		Status:  v1.ConditionFalse,
		Reason:  v1.PodReasonUnschedulable,
		Message: tmpReason,
	}
	for _, tmp := range task.Pod.Status.Conditions {
		if strings.Contains(tmp.Message, tmpReason) {
			return
		}
	}
	task.Pod.Status.Conditions = append(task.Pod.Status.Conditions, condition)
}

func updatePodsPendingReason(job *api.JobInfo, tID api.TaskID, reason string) {
	if tID != "" {
		if t, ok := job.Tasks[tID]; ok {
			updatePodPendingReason(t, reason)
			return
		}
		return
	}
	for _, task := range job.Tasks {
		updatePodPendingReason(task, reason)
	}
}

func (sh *ScheduleHandler) SetJobPendingReason(vcJob *api.JobInfo, reason interface{}) error {
	if sh == nil || vcJob == nil {
		return errors.New(util.ArgumentError)
	}
	var tmpReason string
	switch value := reason.(type) {
	case string:
		vcJob.JobFitErrors = value
		tmpReason = value
	case map[api.TaskID]*api.FitErrors:
		vcJob.NodesFitErrors = value
		for _, nodeErrors := range value {
			tmpReason += nodeErrors.Error()
		}
	default:
		return fmt.Errorf("unsupported type of reason")
	}
	sh.UpdatePodGroupPendingReason(vcJob, tmpReason)
	return nil
}

func (sh *ScheduleHandler) UpdatePodGroupPendingReason(job *api.JobInfo, reason string) {
	job.JobFitErrors = reason
	if len(job.PodGroup.Status.Conditions) == 0 {
		return
	}

	jobCondition := job.PodGroup.Status.Conditions[0].DeepCopy()
	jobCondition.Type = util.PodGroupUnschedulableType
	jobCondition.Status = v1.ConditionTrue
	jobCondition.LastTransitionTime = metav1.Now()
	jobCondition.TransitionID = string(sh.SessionID)
	jobCondition.Reason = reason
	jobCondition.Message = reason

	for k, value := range job.PodGroup.Status.Conditions {
		if strings.Contains(value.Message, reason) {
			job.PodGroup.Status.Conditions[k].LastTransitionTime = jobCondition.LastTransitionTime
			job.PodGroup.Status.Conditions[k].TransitionID = jobCondition.TransitionID
			return
		}
	}
	job.PodGroup.Status.Conditions = append(job.PodGroup.Status.Conditions, *jobCondition)
}

func (sh ScheduleHandler) SetJobPendingReasonByNodesCase(job *api.JobInfo) {
	if int32(len(job.Tasks)-len(job.JobFitErrors)) >= job.MinAvailable {
		return
	}
	_ = sh.SetJobPendingReason(job, job.NodesFitErrors)
}

func IsJobInitial(job *api.JobInfo) bool {
	return job.ValidTaskNum() >= job.MinAvailable && getJobTerminatingPodNum(job) == 0
}

func getJobTerminatingPodNum(job *api.JobInfo) int {
	num := 0
	for _, task := range job.Tasks {
		if task.Pod != nil && task.Pod.DeletionTimestamp != nil {
			num++
		}
	}
	return num
}
