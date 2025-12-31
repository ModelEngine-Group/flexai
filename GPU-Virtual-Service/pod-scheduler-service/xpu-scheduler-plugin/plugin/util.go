/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

// Package plugin implements xpu scheduler plugin
package plugin

import (
	"fmt"
	"reflect"
	"strconv"
	"strings"

	"k8s.io/api/core/v1"
	"k8s.io/klog/v2"
	"volcano.sh/volcano/pkg/scheduler/api"
	"volcano.sh/volcano/pkg/scheduler/plugins/xpu-scheduler-plugin/common"
	"volcano.sh/volcano/pkg/scheduler/plugins/xpu-scheduler-plugin/util"
)

// EncodeNodeDevices encode a node's xpus info to string
func EncodeNodeDevices(xpuDevices []*common.XPUDevice) string {
	var encodeNodeDevices strings.Builder
	for _, val := range xpuDevices {
		encodeNodeDevices.Write([]byte(strconv.Itoa(val.Index)))
		encodeNodeDevices.Write([]byte(","))
		encodeNodeDevices.Write([]byte(val.Id))
		encodeNodeDevices.Write([]byte(","))
		encodeNodeDevices.Write([]byte(strconv.Itoa(val.Count)))
		encodeNodeDevices.Write([]byte(","))
		encodeNodeDevices.Write([]byte(strconv.Itoa(int(val.Memory))))
		encodeNodeDevices.Write([]byte(","))
		encodeNodeDevices.Write([]byte(val.Type))
		encodeNodeDevices.Write([]byte(","))
		encodeNodeDevices.Write([]byte(strconv.FormatBool(val.Health)))
		encodeNodeDevices.Write([]byte(":"))
	}
	klog.V(util.LogDebugLevel).Infof("Encode node Devices: %s", encodeNodeDevices.String())
	return encodeNodeDevices.String()
}

// DecodeNodeDevices decode string to node's xpus info
func DecodeNodeDevices(str string, nodeId string) map[int]*common.XPUDevice {
	xpuDevices := make(map[int]*common.XPUDevice)
	if !strings.Contains(str, ":") {
		klog.V(util.LogErrorLevel).Infof("Decode node device failed, wrong annotations: %s", str)
		return xpuDevices
	}
	tmp := strings.Split(str, ":")
	for _, val := range tmp {
		if strings.Contains(val, ",") {
			// NodeDevice
			// Field description: GPU Index, GPU UUID, count, memory, type, health
			items := strings.Split(val, ",")
			if len(items) != util.XPUDeviceLen {
				klog.V(util.LogErrorLevel).Infof("Decode node device failed, wrong device info: %s", val)
				return map[int]*common.XPUDevice{}
			}
			index, err := strconv.Atoi(items[0])
			count, err := strconv.Atoi(items[2])
			memory, err := strconv.Atoi(items[3])
			health, err := strconv.ParseBool(items[5])
			numa, err := strconv.Atoi(items[6])
			if err != nil {
				klog.V(util.LogErrorLevel).Infof("Decode node device failed, wrong device info: %s", val)
				return map[int]*common.XPUDevice{}
			}
			i := &common.XPUDevice{
				Index:      index,
				Id:         items[1],
				NodeId:     nodeId,
				Type:       items[4],
				Count:      count,
				Health:     health,
				Cores:      util.Base100,
				Memory:     uint64(memory),
				UsedCores:  0,
				UsedMemory: 0,
				UsedVids:   0,
				InUse:      false,
				Numa:       numa,
			}
			xpuDevices[index] = i
		}
	}
	return xpuDevices
}

// EncodeContainerDevices encode vxpu resource request of a container to string
func EncodeContainerDevices(cd ContainerDevices) string {
	var encodeContainerDevices strings.Builder
	for _, val := range cd {
		encodeContainerDevices.Write([]byte(strconv.Itoa(int(val.Index))))
		encodeContainerDevices.Write([]byte(","))
		encodeContainerDevices.Write([]byte(val.Id))
		encodeContainerDevices.Write([]byte(","))
		valType := val.Type
		if strings.Contains(valType, util.NvidiaGPUDevice) {
			valType = util.NvidiaGPUDevice
		}
		if strings.Contains(valType, util.AscendNPUDevice) {
			valType = util.AscendNPUDevice
		}
		encodeContainerDevices.Write([]byte(valType))
		encodeContainerDevices.Write([]byte(","))
		encodeContainerDevices.Write([]byte(strconv.Itoa(int(val.UsedMemory))))
		encodeContainerDevices.Write([]byte(","))
		encodeContainerDevices.Write([]byte(strconv.Itoa(int(val.UsedCores))))
		encodeContainerDevices.Write([]byte(","))
		encodeContainerDevices.Write([]byte(strconv.FormatUint(uint64(val.Vid), util.Base10)))
		encodeContainerDevices.Write([]byte(":"))
	}
	klog.V(util.LogDebugLevel).Infof("Encode container Devices: %s", encodeContainerDevices.String())
	return encodeContainerDevices.String()
}

// EncodePodDevices encode vxpu resource request of a pod to string
func EncodePodDevices(pd PodDevices) string {
	var ss []string
	for _, cd := range pd {
		ss = append(ss, EncodeContainerDevices(cd))
	}
	return strings.Join(ss, ";")
}

// DecodeContainerDevices decode vxpu resource request of a container from string
func DecodeContainerDevices(str string) ContainerDevices {
	if len(str) == 0 {
		return ContainerDevices{}
	}
	cd := strings.Split(str, ":")
	containerDevices := ContainerDevices{}
	for _, val := range cd {
		if strings.Contains(val, ",") == false {
			continue
		}
		fields := strings.Split(val, ",")
		tmpdev := common.ContainerDevice{}
		if len(fields) != reflect.TypeOf(tmpdev).NumField() {
			klog.V(util.LogErrorLevel).Infof("DecodeContainerDevices invalid parameter: %s", str)
			return ContainerDevices{}
		}
		index, err := strconv.Atoi(fields[0])
		if err != nil {
			klog.V(util.LogErrorLevel).Infof("DecodeContainerDevices invalid parameter: %s", str)
			return ContainerDevices{}
		}
		tmpdev.Index = index
		tmpdev.Id = fields[1]
		tmpdev.Type = fields[2]
		mem, err := strconv.Atoi(fields[3])
		if err != nil {
			klog.V(util.LogErrorLevel).Infof("DecodeContainerDevices invalid parameter: %s", str)
			return ContainerDevices{}
		}
		tmpdev.UsedMemory = uint64(mem)
		devcores, err := strconv.Atoi(fields[4])
		if err != nil {
			klog.V(util.LogErrorLevel).Infof("DecodeContainerDevices invalid parameter: %s", str)
			return ContainerDevices{}
		}
		tmpdev.UsedCores = devcores
		vid, err := strconv.ParseUint(fields[5], util.Base10, 0)
		if err != nil {
			klog.V(util.LogErrorLevel).Infof("DecodeContainerDevices invalid parameter: %s", str)
			return ContainerDevices{}
		}
		tmpdev.Vid = uint(vid)
		containerDevices = append(containerDevices, tmpdev)
	}
	return containerDevices
}

// DecodePodDevices decode vxpu resource request of a pod from string
func DecodePodDevices(str string) PodDevices {
	if len(str) == 0 {
		return PodDevices{}
	}
	var pd PodDevices
	for _, s := range strings.Split(str, ";") {
		cd := DecodeContainerDevices(s)
		pd = append(pd, cd)
	}
	return pd
}

// GetXPUDevicesNotInUse calculate unused xou devices on nodes for use in topology scheduling.
// Unused devices refer to those that have not been allocated vxpu resources
// and are not occupied by topology scheduling for other jobs
func GetXPUDevicesNotInUse(xpuDevices map[int]*common.XPUDevice,
	inUseDevices map[string]map[int]struct{}, nodeName string) []*common.XPUDevice {
	var res []*common.XPUDevice
	inUseDevicesOfNode, ok := inUseDevices[nodeName]
	if !ok {
		inUseDevicesOfNode = map[int]struct{}{}
	}
	for _, v := range xpuDevices {
		_, usedInTopologyAllocation := inUseDevicesOfNode[v.Index]
		if !v.InUse && v.Health && !usedInTopologyAllocation {
			res = append(res, v)
		}
	}
	return res
}

// UpdateXPUDevicesFromTopologyResults update the xpuDevices struct, setting xpuDevice that are not yet bound
// to a pod but are occupied by topology allocation to exclusive use.
func UpdateXPUDevicesFromTopologyResults(xpuDevices map[int]*common.XPUDevice, inUseDevices map[int]struct{}) {
	if inUseDevices == nil || len(inUseDevices) == 0 {
		return
	}
	for index := range inUseDevices {
		if _, exist := xpuDevices[index]; !exist {
			continue
		}
		xpuDevices[index].InUse = true
		xpuDevices[index].UsedCores = util.Base100
		xpuDevices[index].UsedMemory = xpuDevices[index].Memory
	}
}

func initScoreMap(nodes []*api.NodeInfo) map[string]float64 {
	scoreMap := make(map[string]float64, len(nodes))
	for _, node := range nodes {
		if reflect.ValueOf(node).IsNil() {
			continue
		}
		scoreMap[node.Name] = 0.0
	}
	return scoreMap
}

// GetXPUDevicesFromTopologyScheduleResult get xpu devices from topology schedule result
func GetXPUDevicesFromTopologyScheduleResult(sJobs map[api.JobID]*SchedulerJob) map[string]map[int]struct{} {
	inUseDevicesOfTopology := make(map[string]map[int]struct{})
	for _, v := range sJobs {
		for _, x := range v.TopologyScheduleResult {
			if _, ok := inUseDevicesOfTopology[x.NodeName]; !ok {
				inUseDevicesOfTopology[x.NodeName] = make(map[int]struct{})
			}
			for _, y := range x.AllocateXPUs {
				inUseDevicesOfTopology[x.NodeName][y] = struct{}{}
			}
		}
	}
	return inUseDevicesOfTopology
}

// getContainerDevices get container device info
func getContainerDevices(allocateXPUs []int, xpuDevices map[int]*common.XPUDevice) (ContainerDevices, error) {
	cds := ContainerDevices{}
	for _, v := range allocateXPUs {
		xpuDevice, ok := xpuDevices[v]
		if !ok {
			return nil, fmt.Errorf("getContainerDevices failed, XPU %d does not exist on the node", v)
		}
		cd := common.ContainerDevice{
			Index:      xpuDevice.Index,
			Id:         xpuDevice.Id,
			Type:       xpuDevice.Type,
			UsedMemory: xpuDevice.Memory,
			UsedCores:  util.Base100,
		}
		cds = append(cds, cd)
	}
	return cds, nil
}

// GetXPUResourceFromTaskInfo for get xpu resource info from task info
func GetXPUResourceFromTaskInfo(task *api.TaskInfo, xpuName string) *util.TaskResource {
	taskResource := &util.TaskResource{
		ReqXPUName:          "",
		ReqXPUNum:           0,
		ReqXPUType:          "",
		ReqXPUCores:         0,
		ReqXPUMem:           0,
		ReqXPUMemPercentage: 0,
	}
	xpuCoreName, xpuMemName, xpuTypeName := "", "", ""
	if xpuName == util.VGPUName {
		xpuCoreName, xpuMemName, xpuTypeName = util.VGPUCore, util.VGPUMemory, util.VGPUType
	} else {
		xpuCoreName, xpuMemName, xpuTypeName = util.VNPUCore, util.VNPUMemory, util.VNPUType
	}
	for _, container := range task.Pod.Spec.Containers {
		containerResource := GetXPUResourceFromContainer(&container, xpuName, xpuCoreName, xpuMemName, xpuTypeName)
		if containerResource.ReqXPUNum == 0 {
			continue
		}
		taskResource.ReqXPUNum += containerResource.ReqXPUNum
		taskResource.ReqXPUCores += containerResource.ReqXPUCores * containerResource.ReqXPUNum
		taskResource.ReqXPUMem += containerResource.ReqXPUMem * containerResource.ReqXPUNum
		taskResource.ReqXPUMemPercentage += containerResource.ReqXPUMemPercentage * containerResource.ReqXPUNum
		// all the containers' xpu device type of the pod must be the same
		if taskResource.ReqXPUType == "" {
			taskResource.ReqXPUType = containerResource.ReqXPUType
		}
	}
	return taskResource
}

// GetXPUResourceFromContainer for get xpu resource info from container
func GetXPUResourceFromContainer(container *v1.Container, xpuName string, xpuCoreName string,
	xpuMemName string, xpuTypeName string) util.ContainerResource {
	containerResource := util.ContainerResource{
		ReqXPUName:          xpuName,
		ReqXPUNum:           0,
		ReqXPUType:          "",
		ReqXPUCores:         0,
		ReqXPUMem:           0,
		ReqXPUMemPercentage: 0,
	}
	vxpuNum := util.GetVXPUResource(container, xpuName)
	vxpuCore := util.GetVXPUResource(container, xpuCoreName)
	vxpuMem := util.GetVXPUResource(container, xpuMemName) * util.Base1024
	if vxpuNum == 0 {
		klog.V(util.LogDebugLevel).Infof("Container %s do not apply xpu device, resources limit: %v",
			container.Name, container.Resources.Limits)
		return containerResource
	}
	containerResource.ReqXPUNum = vxpuNum
	if vxpuCore == 0 && vxpuMem == 0 {
		// if num > 0, core = 0, memory = 0, apply the entire card.
		containerResource.ReqXPUCores = util.Base100
		containerResource.ReqXPUMemPercentage = util.Base100
	} else if vxpuCore != 0 && vxpuMem == 0 {
		// if num > 0, core > 0, memory = 0, apply all the memory of the card.
		containerResource.ReqXPUCores = vxpuCore
		containerResource.ReqXPUMemPercentage = util.Base100
	} else {
		// if num > 0, core > 0, memory > 0, normal apply the card.
		containerResource.ReqXPUCores = vxpuCore
		containerResource.ReqXPUMem = vxpuMem
	}
	containerResource.ReqXPUType = util.GetXPUType(container, xpuTypeName)
	return containerResource
}
