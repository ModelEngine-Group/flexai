/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

package plugin

import (
	"errors"
	"strings"
	"sync"

	"volcano.sh/volcano/pkg/scheduler/api"
	"volcano.sh/volcano/pkg/scheduler/plugins/xpu-scheduler-plugin/util"
)

func (sJob *SchedulerJob) Init(vcJob *api.JobInfo, sh *ScheduleHandler) error {
	if sJob == nil || vcJob == nil {
		return errors.New("parameter is null")
	}
	if initErr := sJob.initJobInfo(vcJob); initErr != nil {
		return initErr
	}

	if !sJob.isJobSupportByPlugin(sh) {
		return errors.New("job is not support by plugin")
	}

	sJob.initPluginByJobInfo(sh)
	return nil
}

func (sJob *SchedulerJob) initJobInfo(vcJob *api.JobInfo) error {
	name, num, tasks := getJobXPUTasks(vcJob)
	if tasks == nil {
		return errors.New("job has no xpu task")
	}
	sJob.JobReadyTag = true
	sJob.UnschedulableReason = UnschedulableReason{Reason: map[string]string{}, Mutex: &sync.Mutex{}}
	sJob.Id = vcJob.UID
	sJob.NameSpace = vcJob.Namespace
	sJob.ReferenceName = util.ReferenceNameOfJob(vcJob)
	sJob.Selector = getSelectorFromVcJob(vcJob)
	sJob.Label = getLabelFromVcJob(vcJob)
	sJob.Annotation = vcJob.PodGroup.Annotations
	sJob.handler = nil
	sJob.XPUJob = &util.XPUJob{
		ReqXPUName:             name,
		ReqXPUNum:              num,
		Tasks:                  tasks,
		ReqXPUInterBandwidth:   GetXPUTopologyInterBandwidth(sJob),
		TopologyScheduleResult: make(map[api.TaskID]*util.TopologyScheduleXPUs),
	}
	if name == "" {
		return errors.New("job has no xpu task")
	}
	sJob.XPUTaskNum = sJob.GetXPUTaskNumInJob()
	if vcJob.MinAvailable != int32(len(vcJob.Tasks)) {
		sJob.SchedulingTaskNum = defaultSchedulingTaskNum
		return nil
	}
	sJob.SchedulingTaskNum = sJob.GetSchedulingTaskNum()
	return nil
}

func (sJob *SchedulerJob) isJobSupportByPlugin(sh *ScheduleHandler) bool {
	name := sJob.getPluginNameByReq()
	if name == "" {
		return false
	}
	return sh.IsPluginRegistered(name)
}

func isSelectorContains(value string, jobValue string) bool {
	for _, v := range strings.Split(value, "|") {
		if strings.EqualFold(v, jobValue) {
			return true
		}
	}
	return false
}

func getTaskSelectors(task *api.TaskInfo) map[string]string {
	if task == nil {
		return nil
	}
	return task.Pod.Spec.NodeSelector
}

func getTaskLabels(task *api.TaskInfo) map[string]string {
	if task == nil {
		return nil
	}
	return task.Pod.Labels
}

func getLabel(res map[string]string, taskSelector map[string]string) {
	if res == nil {
		return
	}
	for k, v := range taskSelector {
		label, ok := res[k]
		if !ok {
			res[k] = v
			continue
		}
		if isSelectorContains(label, v) {
			continue
		}
		res[k] = label + "|" + v
	}
}

func getLabelFromVcJob(job *api.JobInfo) map[string]string {
	if job == nil {
		return nil
	}
	res := make(map[string]string, util.MapInitNum)
	for labelKey, labelValue := range job.PodGroup.Labels {
		res[labelKey] = labelValue
	}
	for _, task := range job.Tasks {
		taskSelector := getTaskLabels(task)
		getLabel(res, taskSelector)
	}
	return res
}

func getSelectorFromVcJob(job *api.JobInfo) map[string]string {
	var res = make(map[string]string, util.MapInitNum)
	for _, task := range job.Tasks {
		taskSelector := task.Pod.Spec.NodeSelector
		getLabel(res, taskSelector)
	}
	return res
}

func getTaskResource(task *api.TaskInfo) *util.TaskResource {
	taskResources := GetXPUResourceFromTaskInfo(task, util.VGPUName)
	if taskResources.ReqXPUNum != 0 {
		taskResources.ReqXPUName = util.VGPUName
	} else {
		taskResources = GetXPUResourceFromTaskInfo(task, util.VNPUName)
		if taskResources.ReqXPUNum != 0 {
			taskResources.ReqXPUName = util.VNPUName
		}
	}
	return taskResources
}

func getJobXPUTasks(vcJob *api.JobInfo) (string, int, map[api.TaskID]*util.XPUTask) {
	if vcJob == nil {
		return "", 0, nil
	}
	if len(vcJob.Tasks) == 0 {
		return "", 0, nil
	}

	name := ""
	num := 0
	resultMap := make(map[api.TaskID]*util.XPUTask, util.MapInitNum)
	for taskID, taskInf := range vcJob.Tasks {
		taskResource := getTaskResource(taskInf)
		isVXPUTask := false
		if taskResource.ReqXPUCores != taskResource.ReqXPUNum*util.Base100 ||
			taskResource.ReqXPUMemPercentage != taskResource.ReqXPUNum*util.Base100 {
			isVXPUTask = true
		}
		resultMap[taskID] = &util.XPUTask{
			Name:                 taskInf.Name,
			Namespace:            taskInf.Namespace,
			TaskResource:         taskResource,
			IsVXPUTask:           isVXPUTask,
			NodeName:             taskInf.NodeName,
			Annotation:           taskInf.Pod.Annotations,
			PodStatus:            taskInf.Pod.Status.Phase,
			ReqXPUIntraBandwidth: GetXPUTopologyIntraBandwidth(taskInf.Pod),
			ScoreMap:             make(map[string]float64),
			Selector:             getTaskSelectors(taskInf),
			Label:                getTaskLabels(taskInf),
		}
		if name == "" {
			name = taskResource.ReqXPUName
		} else if taskResource.ReqXPUName != "" && name != taskResource.ReqXPUName {
			return "", 0, nil
		}
		num += taskResource.ReqXPUNum
	}
	return name, num, resultMap
}

func (sJob *SchedulerJob) initPluginByJobInfo(sh *ScheduleHandler) {
	if sJob == nil {
		return
	}
	pluginName := sJob.getPluginNameByReq()
	if pluginName == "" {
		return
	}
	plugin, ok := sh.XPUPlugins[pluginName]
	if !ok {
		return
	}
	sJob.handler = plugin()
}

func (sJob *SchedulerJob) getPluginNameByReq() string {
	name := sJob.ReqXPUName
	if strings.Contains(name, util.VGPUName) {
		return util.GPUPluginName
	}
	if strings.Contains(name, util.VNPUName) {
		return util.NPUPluginName
	}
	return ""
}

func (sJob *SchedulerJob) ValidJobFn() *api.ValidateResult {
	if result := sJob.handler.ValidXPUJob(); result != nil {
		return result
	}
	return nil
}

func (sJob *SchedulerJob) preCheckNodePredicate(taskInfo *api.TaskInfo, nodeInfo *api.NodeInfo) error {
	if !util.IsSelectorMeetJob(sJob.Selector, nodeInfo.Node.Labels) {
		meetErr := errors.New("node labels not meet job selector")
		return meetErr
	}
	return nil
}
