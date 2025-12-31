/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

package main

import (
	"errors"
	"strings"
	"sync"

	"k8s.io/klog/v2"
	"volcano.sh/volcano/pkg/scheduler/api"
	"volcano.sh/volcano/pkg/scheduler/framework"
	"volcano.sh/volcano/pkg/scheduler/plugins/xpu-scheduler-plugin/common"
	"volcano.sh/volcano/pkg/scheduler/plugins/xpu-scheduler-plugin/internal/xpu"
	"volcano.sh/volcano/pkg/scheduler/plugins/xpu-scheduler-plugin/plugin"
	"volcano.sh/volcano/pkg/scheduler/plugins/xpu-scheduler-plugin/util"
)

const (
	// PluginName huawei xpu plugin name, it must be the same as the so package name
	PluginName = "huawei-xpu"
	// TopologyEnable topology setting
	TopologyEnable = "TopologyEnable"
	// NumaEnable numa setting
	NumaEnable = "NumaEnable"
	// TestEnable test mode setting
	TestEnable = "TestEnable"
	// XPUTopologyNodeList node list setting
	XPUTopologyNodeList = "XPUTopologyNodeList"
	// XPUTopologyNodeBandwidth bandwidth setting between nodes
	XPUTopologyNodeBandwidth = "XPUTopologyNodeBandwidth"
)

var (
	scheduleHandler *plugin.ScheduleHandler
	once            sync.Once
)

// GetScheduleHandler implement the singleton pattern of the ScheduleHandler
func GetScheduleHandler() *plugin.ScheduleHandler {
	once.Do(func() {
		scheduleHandler = HandlerCreate()
	})
	return scheduleHandler
}

type huaweiXPUPlugin struct {
	// Scheduler for plugin args and its handler.
	Scheduler *plugin.ScheduleHandler
	// Arguments given for the plugin
	Arguments framework.Arguments
}

func (xp *huaweiXPUPlugin) Name() string {
	return PluginName
}

// New huawei xpu framework plugin
func New(arguments framework.Arguments) framework.Plugin {
	return &huaweiXPUPlugin{Scheduler: GetScheduleHandler(), Arguments: arguments}
}

func getCommonConfig(args framework.Arguments) {
	args.GetBool(&xpu.Config.TopologyEnable, TopologyEnable)
	args.GetBool(&xpu.Config.NumaEnable, NumaEnable)
	args.GetBool(&xpu.Config.TestEnable, TestEnable)
}

func getNodeBandwidthConf(args framework.Arguments) {
	argv, ok := args[XPUTopologyNodeList]
	if !ok {
		return
	}
	value, ok := argv.(string)
	if !ok {
		klog.V(util.LogErrorLevel).Infof("XPUTopologyNodeList in args is not string")
		return
	}
	tmp := strings.Split(value, util.Comma)
	topologyNodeList := tmp

	err := getNodeBandwidth(args, topologyNodeList)
	if err != nil {
		klog.V(util.LogErrorLevel).Infof("get node bandwidth failed, err: %v", err.Error())
		util.XPUTopologyNodeBandwidth = nil
	}
	return
}

func getNodeBandwidth(args framework.Arguments, topologyNodeList []string) error {
	argv, ok := args[XPUTopologyNodeBandwidth]
	if !ok {
		return errors.New("XPUTopologyNodeBandwidth not exist")
	}
	value, ok := argv.(string)
	if !ok {
		return errors.New("XPUTopologyNodeBandwidth is not string")
	}
	matrix := strings.Split(value, util.Semicolon)
	if len(matrix) != len(topologyNodeList) {
		return errors.New("length of node bandwidth matrix is different from length of node list")
	}
	nodeBandWidth, err := util.ConvertMatrix2Map(matrix, topologyNodeList)
	if err != nil {
		util.XPUTopologyNodeBandwidth = nil
		return err
	}
	util.XPUTopologyNodeBandwidth = nodeBandWidth
	klog.V(util.LogInfoLevel).Infof("XPUTopologyNodeBandwidth: +%v", util.XPUTopologyNodeBandwidth)
	return nil
}

func addJobValidFn(ssn *framework.Session, xp *huaweiXPUPlugin) {
	// check job npu resource, if illegal return failed
	ssn.AddJobValidFn(xp.Name(), func(obj interface{}) *api.ValidateResult {
		return xp.Scheduler.JobValid(obj)
	})
}

func addPredicateFn(ssn *framework.Session, xp *huaweiXPUPlugin) {
	// if node not meet the task require, the task will be failed. so need to intercept in advance
	ssn.AddPredicateFn(xp.Name(), func(taskInfo *api.TaskInfo, nodeInfo *api.NodeInfo) error {
		err := xp.Scheduler.NodePredicate(taskInfo, nodeInfo)
		if err != nil {
			xp.Scheduler.Jobs[taskInfo.Job].Lock()
			xp.Scheduler.Jobs[taskInfo.Job].Reason[err.Error()] += nodeInfo.Name + " "
			xp.Scheduler.Jobs[taskInfo.Job].Unlock()
		}
		return err
	})
}

func addBatchNodeOrderFn(ssn *framework.Session, xp *huaweiXPUPlugin) {
	ssn.AddBatchNodeOrderFn(xp.Name(), func(task *api.TaskInfo, nodes []*api.NodeInfo) (map[string]float64, error) {
		score, err := xp.Scheduler.BatchNodeOrderFn(task, nodes)
		if err != nil {
			if setErr := xp.Scheduler.SetJobPendingReason(ssn.Jobs[task.Job], err.Error()); setErr != nil {
				klog.V(util.LogErrorLevel).Infof("%s setJobFailed err:%s.", PluginName, util.SafePrint(setErr))
			}
		}
		if vcJob, ok := xp.Scheduler.Jobs[task.Job]; ok && vcJob.JobReadyTag == false {
			if _, exist := xp.Scheduler.DeleteJobInfos[task.Job]; !exist {
				xp.Scheduler.DeleteJobInfos[task.Job] = ssn.Jobs[task.Job]
				delete(ssn.Jobs, task.Job)
			}
		}
		return score, nil
	})
}

func addEventHandler(ssn *framework.Session, xp *huaweiXPUPlugin) {
	// Register event handlers to update task info in PodLister & nodeMap
	// for support Concurrency
	ssn.AddEventHandler(&framework.EventHandler{
		AllocateFunc: func(event *framework.Event) {
			if event == nil {
				klog.V(util.LogErrorLevel).Infof("AllocateFunc event nil.")
				return
			}
			xp.Scheduler.XPUAllocateFunc(event.Task, ssn)
		},
		DeallocateFunc: func(event *framework.Event) {
			if event == nil {
				klog.V(util.LogErrorLevel).Infof("DeallocateFunc event nil.")
				return
			}
			xp.Scheduler.XPUDeallocateFunc(event.Task)
		},
	})
}

func addJobReadyFn(ssn *framework.Session, xp *huaweiXPUPlugin) {
	ssn.AddJobReadyFn(xp.Name(), func(obj interface{}) bool {
		ji, ok := obj.(*api.JobInfo)
		if !ok {
			klog.V(util.LogErrorLevel).Info("obj assertion failed.")
			return false
		}
		job, ok := xp.Scheduler.Jobs[ji.UID]
		if !ok {
			return true
		}
		return job.JobReadyTag
	})
}

func (xp *huaweiXPUPlugin) OnSessionOpen(ssn *framework.Session) {
	klog.V(util.LogDebugLevel).Infof("enter %s OnSessionOpen.", PluginName)
	defer klog.V(util.LogDebugLevel).Infof("leave %s OnSessionOpen.", PluginName)
	if xp == nil || ssn == nil {
		klog.V(util.LogErrorLevel).Infof("OnSessionOpen: %s.", util.ArgumentError)
		return
	}
	getCommonConfig(xp.Arguments)
	getNodeBandwidthConf(xp.Arguments)
	// Init xpu plugin and nodes.
	if err := xp.Scheduler.InitXPUSession(ssn); err != nil {
		klog.V(util.LogErrorLevel).Infof("InitXPUSession: %s, xpu plugin will not be initialized.", err)
		return
	}

	addJobValidFn(ssn, xp)
	addPredicateFn(ssn, xp)
	addBatchNodeOrderFn(ssn, xp)
	addEventHandler(ssn, xp)
	addJobReadyFn(ssn, xp)
}

func (xp *huaweiXPUPlugin) OnSessionClose(ssn *framework.Session) {
	klog.V(util.LogDebugLevel).Infof("enter %s OnSessionClose.", PluginName)
	defer klog.V(util.LogDebugLevel).Infof("leave %s OnSessionClose.", PluginName)
	if xp == nil || ssn == nil {
		klog.V(util.LogErrorLevel).Infof("OnSessionClose failed: %s.", util.ArgumentError)
		return
	}
	if ssn.Jobs == nil && len(xp.Scheduler.DeleteJobInfos) != 0 {
		ssn.Jobs = make(map[api.JobID]*api.JobInfo)
	}
	// 1. Record job's unscheduled reason;
	// 2. Update job statue;
	// 3. Handle other post-dispatch issues.
	for _, job := range ssn.Jobs {
		// deal pending job
		if job.PodGroup.Status.Phase == util.PodGroupInqueue ||
			job.PodGroup.Status.Phase == util.PodGroupPending {
			// if all nodes not meet job require failed
			xp.Scheduler.SetJobPendingReasonByNodesCase(job)
		}
		if len(job.PodGroup.Annotations) != 0 && job.PodGroup.Annotations[util.PodDeleteTimes] == util.TagOfPodPending {
			xp.Scheduler.UpdatePodGroupPendingReason(job, util.JobRestartReason)
		}
	}
	for jobId, jobInfo := range xp.Scheduler.DeleteJobInfos {
		ssn.Jobs[jobId] = jobInfo
	}
}

// HandlerCreate Huawei XPU scheduler plugin start by frame.
func HandlerCreate() *plugin.ScheduleHandler {
	sh := &plugin.ScheduleHandler{
		XPUPlugins:     map[string]plugin.XPUBuilder{},
		XPUDevices:     map[string]map[int]*common.XPUDevice{},
		Jobs:           map[api.JobID]*plugin.SchedulerJob{},
		DeleteJobInfos: map[api.JobID]*api.JobInfo{},
		SessionID:      "",
		Mutex:          &sync.Mutex{},
	}

	// Register new xpu scheduler strategy.
	sh.RegisterXPUScheduler(util.GPUPluginName, xpu.GetGPUPlugin)
	sh.RegisterXPUScheduler(util.NPUPluginName, xpu.GetNPUPlugin)
	klog.V(util.LogDebugLevel).Infof("HandlerCreate %#v.", sh.XPUPlugins)
	return sh
}
