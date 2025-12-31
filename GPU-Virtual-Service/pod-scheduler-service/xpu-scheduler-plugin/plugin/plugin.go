/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

package plugin

import (
	"errors"
	"fmt"
	"strconv"
	"strings"
	"time"

	"k8s.io/api/core/v1"
	"k8s.io/klog/v2"
	"volcano.sh/volcano/pkg/scheduler/api"
	"volcano.sh/volcano/pkg/scheduler/plugins/xpu-scheduler-plugin/allocator"
	"volcano.sh/volcano/pkg/scheduler/plugins/xpu-scheduler-plugin/common"
	"volcano.sh/volcano/pkg/scheduler/plugins/xpu-scheduler-plugin/util"
)

// XPUBuilder PluginBuilder plugin management
type XPUBuilder = func() XPUSchedulerPlugin

// XPUSchedulerPlugin for xpu plugin has function
type XPUSchedulerPlugin interface {
	// ValidXPUJob check if the job part of xpu scheduler policy is valid.
	ValidXPUJob() *api.ValidateResult
	GetXPUDevicesFromNode(*api.NodeInfo) map[int]*common.XPUDevice
	NodePredicateForTask(*SchedulerJob, *api.TaskInfo, *api.NodeInfo, *ScheduleHandler) error
	Allocate(*SchedulerJob, *api.TaskInfo, *api.NodeInfo, map[int]*common.XPUDevice) error
	Deallocate(*api.TaskInfo, *api.NodeInfo) error
}

// SchedulerPlugin for all volcano-npu plugin
type SchedulerPlugin struct {
	PluginName                 string
	VxpuName                   string
	VxpuType                   string
	VxpuCore                   string
	VxpuMemory                 string
	Config                     *CommonConfig
	NodeXPURegisterAnno        string
	AssignedXPUsToAllocateAnno string
	AssignedXPUsToPodAnno      string
	AssignedXPUsToNodeAnno     string
	NodeXPUTopologyAnno        string
	NodeXPUHandshakeAnno       string
}

// CommonConfig for plugin
type CommonConfig struct {
	NumaEnable     bool
	TestEnable     bool
	TopologyEnable bool
}

// ValidXPUJob check job req xpu num
func (sp *SchedulerPlugin) ValidXPUJob() *api.ValidateResult {
	if sp == nil {
		err := errors.New(util.ArgumentError)
		return &api.ValidateResult{Pass: false, Reason: err.Error(), Message: err.Error()}
	}
	return nil
}

func getInUseDevice(inUseDeviceMap map[string][]common.ContainerDevice, annoName string, pod *v1.Pod) {
	if inUseDeviceMap == nil {
		err := errors.New(util.ArgumentError)
		klog.V(util.LogErrorLevel).Infof("getInUseDevice err: %s", err.Error())
		return
	}
	if _, ok := pod.Annotations[annoName]; !ok {
		return
	}
	pdevices := DecodePodDevices(pod.Annotations[annoName])
	for _, v := range pdevices {
		for _, dev := range v {
			if _, ok := inUseDeviceMap[dev.Id]; !ok {
				inUseDeviceMap[dev.Id] = []common.ContainerDevice{}
			}
			inUseDeviceMap[dev.Id] = append(inUseDeviceMap[dev.Id], dev)
		}
	}
}

// GetXPUDevicesFromNode get xpu infos from node
func (sp *SchedulerPlugin) GetXPUDevicesFromNode(node *api.NodeInfo) map[int]*common.XPUDevice {
	infos, ok := node.Node.Annotations[sp.NodeXPURegisterAnno]
	if !ok {
		klog.V(util.LogWarningLevel).Infof("Get XPU Devices failed, annotation %s not exist on node %s",
			sp.NodeXPURegisterAnno, node.Name)
		return nil
	}
	if !sp.Config.TestEnable && !checkHandShake(node, sp.NodeXPUHandshakeAnno) {
		return nil
	}
	xpuDevices := DecodeNodeDevices(infos, node.Name)
	inUseDeviceMap := make(map[string][]common.ContainerDevice)
	for _, pod := range node.Pods() {
		getInUseDevice(inUseDeviceMap, sp.AssignedXPUsToPodAnno, pod)
	}
	for _, v := range xpuDevices {
		if _, ok := inUseDeviceMap[v.Id]; ok {
			for _, x := range inUseDeviceMap[v.Id] {
				v.UsedMemory += x.UsedMemory
				v.UsedCores += x.UsedCores
				v.OccupyVid(x.Vid)
			}
			v.InUse = true
		}
	}
	klog.V(util.LogDebugLevel).Infof("Get XPU Device info for Node %s, device num: %d", node.Name, len(xpuDevices))
	return xpuDevices
}

func checkHandShake(node *api.NodeInfo, anno string) bool {
	handshake, ok := node.Node.Annotations[anno]
	if !ok {
		klog.V(util.LogWarningLevel).Infof("Check Handshake failed, annotation %s not exist on node %s",
			anno, node.Name)
		return false
	}
	if strings.Contains(handshake, "Reported") {
		loc, err := time.LoadLocation("Asia/Shanghai")
		if err != nil {
			klog.V(util.LogWarningLevel).Infof("time load location error: %v", err)
		}
		formertime, err := time.ParseInLocation("2006.01.02 15:04:05", strings.Split(handshake, "_")[1], loc)
		if err != nil {
			klog.V(util.LogWarningLevel).Infof("Handshake time parse error: %v", err)
			return false
		}
		if time.Now().After(formertime.Add(time.Second * util.HandshakeTolerateUpdateTime)) {
			klog.V(util.LogWarningLevel).Infof("node %v Handshake has not been updated for %v seconds",
				node.Name, util.HandshakeTolerateUpdateTime)
			return false
		}
	} else {
		klog.V(util.LogWarningLevel).Infof("Handshake dose not include Reported: %v", handshake)
		return false
	}
	return true
}

// NodePredicateForTask evaluate whether node meet the task requirement based on different strategies in both
// topology and non-topology scenarios
func (sp *SchedulerPlugin) NodePredicateForTask(sJob *SchedulerJob, task *api.TaskInfo,
	node *api.NodeInfo, sh *ScheduleHandler) error {
	if sp == nil || sJob == nil || task == nil || len(node.Node.Annotations) == 0 {
		err := errors.New(util.ArgumentError)
		klog.V(util.LogErrorLevel).Infof("NodePredicateForTask err: %v", err.Error())
		return err
	}

	xpuTask, ok := sJob.Tasks[task.UID]
	if !ok {
		return fmt.Errorf("node predicte failed, task %s is not exist in job %s",
			task.Name, sJob.Id)
	}

	unUseXPUDevivesOfNodes := sh.GetAllocatableXPUDeviceOnNodes(sJob)

	// node predicate for topology task
	if sp.Config.TopologyEnable && !xpuTask.IsVXPUTask {
		// run core
		sJob.TopologyAllocateOnce.Do(func() {
			sp.PerformTopologyAllocation(sh.Nodes, task, sJob, unUseXPUDevivesOfNodes)
		})
		result := sJob.TopologyScheduleResult[task.UID]

		if result == nil || result.NodeName != node.Name {
			return errors.New("different from the topology scheduling pre-allocation result")
		}
		return nil
	}

	// node predicate for topology task
	var score float64
	fit, _, err := sp.calculateDecision(task.Pod, sh.getXPUDevicesOfNode(node.Name), &score)
	if err != nil || !fit {
		return fmt.Errorf("%s predicate failed: no suitable devices, err: %v",
			sp.PluginName, err)
	}
	xpuTask.Lock()
	xpuTask.ScoreMap[node.Name] = score
	xpuTask.Unlock()
	klog.V(util.LogDebugLevel).Infof("task[%v] Node[%v] score[%v]", task.Name, node.Name, score)
	return nil
}

// PerformTopologyAllocation Perform topology allocation for all tasks under sJob
// and return the allocation result for the current task
func (sp *SchedulerPlugin) PerformTopologyAllocation(nodes []*api.NodeInfo, task *api.TaskInfo,
	sJob *SchedulerJob, unUseXPUDevicesOfNodes map[string][]*common.XPUDevice) *allocator.PodAllocation {
	allTaskResult, success := sp.topologyAllocate(nodes, unUseXPUDevicesOfNodes, sJob.Tasks, sJob.ReqXPUInterBandwidth)
	if !success {
		return nil
	}
	// reset TopologyScheduleResult of sJob
	sJob.TopologyScheduleResult = make(map[api.TaskID]*util.TopologyScheduleXPUs)

	// Save topology batch scheduling result to sJob.
	// Next time another task(pod) is scheduled, will check the TopologyScheduleResult
	// to avoid executing the topology scheduling again.
	var currentTaskResult *allocator.PodAllocation

	for i, v := range allTaskResult {
		topologyScheduleXPUs := &util.TopologyScheduleXPUs{
			AllocateXPUs: v.DeviceIds,
			NodeName:     v.NodeName,
		}
		sJob.TopologyScheduleResult[v.TaskId] = topologyScheduleXPUs
		if v.TaskId == task.UID {
			currentTaskResult = &allTaskResult[i]
		}
	}
	return currentTaskResult
}

// setXPUDevicesToPod set the selected xpu devices to pod annotation
func (sp *SchedulerPlugin) setXPUDevicesToPod(task *api.TaskInfo, nodeName string, podDevices string) {
	if task == nil || task.Pod == nil || task.Pod.Annotations == nil {
		klog.V(util.LogErrorLevel).Infof("setXPUDevicesToPod err: %s", util.ObjectNilError)
		return
	}

	task.Pod.Annotations[sp.AssignedXPUsToNodeAnno] = nodeName
	task.Pod.Annotations[sp.AssignedXPUsToAllocateAnno] = podDevices
	task.Pod.Annotations[sp.AssignedXPUsToPodAnno] = podDevices
	tmp := strconv.FormatInt(time.Now().UnixNano(), util.Base10)
	task.Pod.Annotations[util.BindTimeAnnotations] = tmp
	task.Pod.Annotations[util.DeviceBindPhase] = util.DeviceBindAllocating
	klog.V(util.LogDebugLevel).Infof("setXPUDevicesToPod %s==%v : %s.", task.Name, tmp, podDevices)
	return
}

// getXPUReqFromContainer get xpu request number from container
func (sp *SchedulerPlugin) getXPUReqFromContainer(container *v1.Container) int {
	var number int = 0
	xpuNum, ok := container.Resources.Limits[v1.ResourceName(sp.VxpuName)]
	if ok {
		number = int(xpuNum.Value())
	}
	return number
}

// getPodDeviceFromAllocateXPUs select xpu devices from node
func (sp *SchedulerPlugin) getPodDeviceFromAllocateXPUs(
	pod *v1.Pod, allocateXPUs []int, xpuDevices map[int]*common.XPUDevice) string {
	selectDevices := PodDevices{}
	start := 0
	length := len(allocateXPUs)
	for _, v := range pod.Spec.Containers {
		xpuNum := sp.getXPUReqFromContainer(&v)
		if xpuNum == 0 {
			continue
		}
		if start+xpuNum > length {
			klog.V(util.LogErrorLevel).Infof("getPodDeviceFromAllocateXPUs failed, insufficient number of xpu devices,"+
				"request xpu number: %d, allocate %v", start+xpuNum, allocateXPUs)
			return ""
		}
		cds, err := getContainerDevices(allocateXPUs[start:start+xpuNum], xpuDevices)
		if err != nil {
			klog.V(util.LogErrorLevel).Infof(
				"getPodDeviceFromAllocateXPUs failed, err: %v", err)
			return ""
		}
		selectDevices = append(selectDevices, cds)
		start += xpuNum
	}
	return EncodePodDevices(selectDevices)
}

func (sp *SchedulerPlugin) getSelectXPUs(task *api.TaskInfo, xpuDevices map[int]*common.XPUDevice) string {
	fit, device, err := sp.calculateDecision(task.Pod, xpuDevices, nil)
	if err != nil || !fit {
		klog.V(util.LogErrorLevel).Infof("%s Allocate failed: no suitable devices was selected.",
			sp.PluginName)
		return ""
	}
	return EncodePodDevices(device)
}

func (sp *SchedulerPlugin) getTopologySelectXPUs(sJob *SchedulerJob, task *api.TaskInfo,
	node *api.NodeInfo, xpuDevices map[int]*common.XPUDevice) string {
	var allocateXPUs []int
	var err error

	topologyScheduleXPUs, exist := sJob.TopologyScheduleResult[task.UID]
	if !exist {
		klog.V(util.LogErrorLevel).Infof("getTopologySelectXPUs err: %v", err)
		return ""
	}
	if topologyScheduleXPUs.NodeName != node.Name {
		klog.V(util.LogErrorLevel).Infof("%s Allocate failed: topology schedule result is incorrect, "+
			"topology node: %s != %s", sp.PluginName, topologyScheduleXPUs.NodeName, node.Name)
		return ""
	}
	allocateXPUs = topologyScheduleXPUs.AllocateXPUs
	return sp.getPodDeviceFromAllocateXPUs(task.Pod, allocateXPUs, xpuDevices)
}

// Allocate select xpu for task from node
func (sp *SchedulerPlugin) Allocate(sJob *SchedulerJob, task *api.TaskInfo,
	node *api.NodeInfo, xpuDevices map[int]*common.XPUDevice) error {
	if sJob == nil || sp == nil || task == nil || node == nil {
		err := errors.New(util.ArgumentError)
		klog.V(util.LogErrorLevel).Infof("%s Allocate err: %s", sp.PluginName, err.Error())
		return err
	}
	selectedXPUs := ""
	if _, ok := sJob.Tasks[task.UID]; !ok {
		klog.V(util.LogErrorLevel).Infof("Allocate task %s is not exist in job %s.",
			task.Name, sJob.Id)
		return nil
	}
	// If the topology scheduling is enabled and the current task is not a xnpu task
	// then execute the topology scheduling process
	if sp.Config.TopologyEnable && !sJob.Tasks[task.UID].IsVXPUTask {
		selectedXPUs = sp.getTopologySelectXPUs(sJob, task, node, xpuDevices)
	} else {
		selectedXPUs = sp.getSelectXPUs(task, xpuDevices)
	}
	if selectedXPUs == "" {
		klog.V(util.LogErrorLevel).Infof("%s Allocate failed: no suitable xpus selected.",
			sp.PluginName)
		return nil
	}
	klog.V(util.LogDebugLevel).Infof("%s Allocate task<%s> select xpu <%v>",
		sp.PluginName, task.Name, selectedXPUs)
	sp.setXPUDevicesToPod(task, node.Name, selectedXPUs)
	return nil
}

// Deallocate remove xpu allocation from node
func (sp *SchedulerPlugin) Deallocate(_ *api.TaskInfo, _ *api.NodeInfo) error {
	return nil
}

// topologyAllocate for topology allocate
func (sp *SchedulerPlugin) topologyAllocate(nodes []*api.NodeInfo,
	unUseXPUDevicesOfNodes map[string][]*common.XPUDevice,
	tasks map[api.TaskID]*util.XPUTask,
	reqXPUInterBandwidth map[string]map[string]int) ([]allocator.PodAllocation, bool) {
	topologyOfNodes := sp.getXPUTopology(nodes, unUseXPUDevicesOfNodes)
	if len(topologyOfNodes) == 0 {
		klog.V(util.LogErrorLevel).Infof("topologyAllocate all nodes have no topology, skip topology scheduling.")
		return nil, false
	}

	podRequests, taskList := sp.buildSchedulingRequest(tasks)
	klog.V(util.LogDebugLevel).Infof(
		"topologyAllocate start, node topology: %v, podrequest: %v, taskList: %v",
		topologyOfNodes, podRequests, taskList)
	// Batch scheduling all tasks within the job at once
	allocator.SetNumaConfig(sp.Config.NumaEnable)
	result, err := allocator.Allocate(topologyOfNodes, podRequests, reqXPUInterBandwidth)
	klog.V(util.LogDebugLevel).Infof("topologyAllocate end, result: %v", result)
	if err != nil {
		klog.V(util.LogErrorLevel).Infof("topologyAllocate failed, err: %v", err)
		return nil, false
	}
	if len(taskList) != len(result) {
		klog.V(util.LogErrorLevel).Infof(
			"topologyAllocate topology scheduling failed, err: result num is not equal to task num")
		return nil, false
	}
	return result, true
}

// getXPUTopology for get xpu topology info
func (sp *SchedulerPlugin) getXPUTopology(
	nodes []*api.NodeInfo, unUseXPUDevicesOfNodes map[string][]*common.XPUDevice) []allocator.NodeResource {
	// Get xpu devices topology info from nodes
	var xpuTopology []allocator.NodeResource
	for _, v := range nodes {
		xpuTopoInfo, ok := v.Node.Annotations[sp.NodeXPUTopologyAnno]
		if !ok {
			klog.V(util.LogDebugLevel).Infof("ScoreBestXPUNodes node %s get xpu topology failed, skip.",
				v.Name)
			continue
		}
		topoGraph, ok := DecodeNodeXPUTopology(xpuTopoInfo)
		if !ok {
			klog.V(util.LogDebugLevel).Infof("ScoreBestXPUNodes node %s decode xpu topology failed, topo info: %s",
				v.Name, xpuTopoInfo)
			continue
		}
		unUseXPUDevices, ok := unUseXPUDevicesOfNodes[v.Name]
		if !ok {
			klog.V(util.LogDebugLevel).Infof("ScoreBestXPUNodes node %s get unuse xpu topology failed, skip.",
				v.Name)
			continue
		}
		unUseDevices := make(map[int]*common.XPUDevice)
		for _, x := range unUseXPUDevices {
			unUseDevices[x.Index] = x
		}
		nodeResource := allocator.NodeResource{
			NodeName:     v.Name,
			Topology:     topoGraph,
			UnuseDevices: unUseDevices,
			CardType:     []string{},
		}
		xpuTopology = append(xpuTopology, nodeResource)
	}
	return xpuTopology
}

// buildSchedulingRequest build topology scheduling request
func (sp *SchedulerPlugin) buildSchedulingRequest(tasks map[api.TaskID]*util.XPUTask) (
	[]allocator.PodCardRequest, []api.TaskID) {
	// Build topology scheduling request
	var podCardRequests []allocator.PodCardRequest
	var taskList []api.TaskID
	i := 0
	for k, v := range tasks {
		// If task is vxpu task skip
		if v.IsVXPUTask {
			continue
		}
		podCardRequest := allocator.PodCardRequest{
			TaskId:         k,
			NumberOfCard:   v.ReqXPUNum,
			IntraBandWidth: v.ReqXPUIntraBandwidth,
			CardType:       v.ReqXPUType,
		}
		if _, ok := v.Annotation[util.TaskSpec]; ok {
			podCardRequest.TaskName = v.Annotation[util.TaskSpec]
		}
		taskList = append(taskList, k)
		podCardRequests = append(podCardRequests, podCardRequest)
		i++
	}
	return podCardRequests, taskList
}
