/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

// Package plugin implements xpu scheduler plugin
package plugin

import (
	"errors"
	"fmt"

	"k8s.io/api/core/v1"
	"k8s.io/klog/v2"
	"volcano.sh/volcano/pkg/scheduler/plugins/xpu-scheduler-plugin/common"
	"volcano.sh/volcano/pkg/scheduler/plugins/xpu-scheduler-plugin/util"
)

func unqualifiedCheck(xpuDevices []*common.XPUDevice, val *util.ContainerResource, i int) bool {
	if i >= len(xpuDevices) {
		return false
	}
	if xpuDevices[i].Count <= int(xpuDevices[i].GetVidBound()) {
		klog.V(util.LogDebugLevel).Infof("Calculate device for container request %v, count is not enough, "+
			"deviceId: %s, max count: %d, used vids: %x",
			val, xpuDevices[i].Id, xpuDevices[i].Count, xpuDevices[i].UsedVids)
		return false
	}
	// If ReqXPUMemPercentage is set and ReqXPUMem is not set, calculate memory with percentage
	if val.ReqXPUMemPercentage != 0 && val.ReqXPUMem == 0 {
		val.ReqXPUMem = int(xpuDevices[i].Memory) * val.ReqXPUMemPercentage / util.Base100
	}
	if xpuDevices[i].Memory-xpuDevices[i].UsedMemory < uint64(val.ReqXPUMem) {
		klog.V(util.LogDebugLevel).Infof("Calculate device for container request %v, memory is not enough, "+
			"deviceId: %s, request memory: %d, exist memory: %d",
			val, xpuDevices[i].Id, val.ReqXPUMem, xpuDevices[i].Memory-xpuDevices[i].UsedMemory)
		return false
	}
	if util.Base100-xpuDevices[i].UsedCores < val.ReqXPUCores {
		klog.V(util.LogDebugLevel).Infof("Calculate device for container request %v, cores is not enough, "+
			"deviceId: %s, request cores: %d, exist cores: %d",
			val, xpuDevices[i].Id, val.ReqXPUCores, util.Base100-xpuDevices[i].UsedCores)
		return false
	}
	// ReqXPUCores=100 indicates it want this card exclusively
	if val.ReqXPUCores == util.Base100 && xpuDevices[i].UsedVids > 0 {
		klog.V(util.LogDebugLevel).Infof("Calculate device for container request %v, skip exclusive card request, "+
			"deviceId: %s, request cores: %d, used vids: %x",
			val, xpuDevices[i].Id, val.ReqXPUCores, xpuDevices[i].UsedVids)
		return false
	}
	// You can't allocate core=0 job to an already full xpu device
	if xpuDevices[i].UsedCores == util.Base100 && val.ReqXPUCores == 0 {
		klog.V(util.LogDebugLevel).Infof("Calculate device for container request %v, skip already full card, "+
			"deviceId: %s, request cores: %d, used cores: %d",
			val, xpuDevices[i].Id, val.ReqXPUCores, xpuDevices[i].UsedCores)
		return false
	}
	// device type must be the same as request xpu type
	if len(val.ReqXPUType) != 0 && xpuDevices[i].Type != val.ReqXPUType {
		klog.V(util.LogDebugLevel).Infof("Calculate device for container request %v, xpu type not the same, "+
			"deviceId: %s, request xpu: %s, device xpu: %s",
			val, xpuDevices[i].Id, val.ReqXPUType, xpuDevices[i].Type)
		return false
	}
	return true
}

// calculate for container xpu device request
func calculate(xpuDevices []*common.XPUDevice, val *util.ContainerResource,
	score *float64) []common.ContainerDevice {
	var cdevs []common.ContainerDevice
	for i := len(xpuDevices) - 1; i >= 0; i-- {
		qualified := unqualifiedCheck(xpuDevices, val, i)
		if !qualified {
			continue
		}
		if val.ReqXPUNum > 0 {
			klog.V(util.LogDebugLevel).Infof("xpu device %s fitted", xpuDevices[i].Id)
			val.ReqXPUNum--
			vid := xpuDevices[i].AllocVid()
			xpuDevices[i].UsedMemory += uint64(val.ReqXPUMem)
			xpuDevices[i].UsedCores += val.ReqXPUCores
			cdevs = append(cdevs, common.ContainerDevice{
				Index:      xpuDevices[i].Index,
				Id:         xpuDevices[i].Id,
				Type:       xpuDevices[i].Type,
				UsedMemory: uint64(val.ReqXPUMem),
				UsedCores:  val.ReqXPUCores,
				Vid:        vid,
			})
			if score != nil {
				*score += util.XpuMultiplier * (float64(xpuDevices[i].UsedMemory) / float64(xpuDevices[i].Memory))
			}
		}
		if val.ReqXPUNum == 0 {
			break
		}
	}
	return cdevs
}

// calculateDecision for calculate pod device decision
func (sp *SchedulerPlugin) calculateDecision(pod *v1.Pod,
	devs map[int]*common.XPUDevice, score *float64) (bool, PodDevices, error) {
	xpuDevices := make([]*common.XPUDevice, len(devs))
	for index, dev := range devs {
		if index >= len(xpuDevices) || index != dev.Index {
			return false, PodDevices{}, errors.New("xpu device index error")
		}
		xpuDevices[index] = dev
	}
	klog.V(util.LogDebugLevel).Infof("Calculate decision for pod %s/%s", pod.Namespace, pod.Name)

	var resourceRequests []*util.ContainerResource
	for _, c := range pod.Spec.Containers {
		containerResource := GetXPUResourceFromContainer(
			&c, sp.VxpuName, sp.VxpuCore, sp.VxpuMemory, sp.VxpuType)
		if containerResource.ReqXPUNum == 0 {
			continue
		}
		if containerResource.ReqXPUCores%coreSplitMinSize != 0 {
			errMsg := fmt.Sprintf("Container %s invalid %s limit: %d",
				c.Name, sp.VxpuCore, containerResource.ReqXPUCores)
			klog.V(util.LogErrorLevel).Infof(errMsg)
			return false, PodDevices{}, fmt.Errorf(errMsg)
		}
		resourceRequests = append(resourceRequests, &containerResource)
	}

	podDevices := PodDevices{}
	for _, val := range resourceRequests {
		if val.ReqXPUNum > len(xpuDevices) {
			return false, PodDevices{}, fmt.Errorf("no enough gpu cards on node, request: %d, have: %d",
				val.ReqXPUNum, len(xpuDevices))
		}
		klog.V(util.LogDebugLevel).Infof("Allocating deivce for container request %v", val)
		cdevs := calculate(xpuDevices, val, score)
		if val.ReqXPUNum > 0 {
			return false, PodDevices{}, fmt.Errorf("no enough gpu fitted on this node")
		}
		podDevices = append(podDevices, cdevs)
	}
	return true, podDevices, nil
}
