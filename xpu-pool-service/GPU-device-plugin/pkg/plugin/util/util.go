/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

// Package util implements util function for device plugin
package util

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"math"
	"reflect"
	"strconv"
	"strings"

	v1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/fields"
	k8stypes "k8s.io/apimachinery/pkg/types"

	"huawei.com/vxpu-device-plugin/pkg/lock"
	"huawei.com/vxpu-device-plugin/pkg/log"
	"huawei.com/vxpu-device-plugin/pkg/plugin/config"
	"huawei.com/vxpu-device-plugin/pkg/plugin/types"
	"huawei.com/vxpu-device-plugin/pkg/plugin/xpu"
)

const (
	deviceLength = 7
	// PodAnnotationMaxLength pod annotation max data length 2MB
	PodAnnotationMaxLength = 1024 * 1024
	// BaseDec base size
	BaseDec = 10
	// BitsSize base size
	BitSize = 64
)

func init() {
	lock.NewClient()
}

// GetNode get k8s node object according to node name
func GetNode(nodename string) (*v1.Node, error) {
	return lock.GetClient().CoreV1().Nodes().Get(context.Background(), nodename, metav1.GetOptions{})
}

// ListPods list k8s pods according to list options
func ListPods(opts metav1.ListOptions) (*v1.PodList, error) {
	return lock.GetClient().CoreV1().Pods("").List(context.Background(), opts)
}

// GetPendingPod get k8s pod object according to node name and types.DeviceBindAllocating status
func GetPendingPod(nodename string) (*v1.Pod, error) {
	podlist, err := ListPods(metav1.ListOptions{})
	if err != nil {
		return nil, err
	}
	var (
		oldestPod      v1.Pod
		oldestBindTime = uint64(math.MaxUint64)
	)
	for _, p := range podlist.Items {
		bindTime, ok := getBindTime(p)
		if !ok {
			continue
		}
		if phase, ok := p.Annotations[types.DeviceBindPhase]; !ok {
			continue
		} else if strings.Compare(phase, types.DeviceBindAllocating) != 0 {
			continue
		}
		if n, ok := p.Annotations[xpu.AssignedNode]; !ok {
			continue
		} else if strings.Compare(n, nodename) == 0 {
			if oldestBindTime > bindTime {
				oldestPod = p
				oldestBindTime = bindTime
			}
		}
	}
	return &oldestPod, nil
}

func getBindTime(pod v1.Pod) (uint64, bool) {
	assumeTimeStr, ok := pod.Annotations[types.DeviceBindTime]
	if !ok {
		return math.MaxUint64, false
	}

	if len(assumeTimeStr) > PodAnnotationMaxLength {
		log.Warningf("timestamp fmt invalid, pod Name: %s", pod.Name)
		return math.MaxUint64, false
	}

	bindTime, err := strconv.ParseUint(assumeTimeStr, BaseDec, BitSize)
	if err != nil {
		log.Errorf("parse timestamp failed, %v", err)
		return math.MaxUint64, false
	}
	return bindTime, true
}

// EncodeNodeDevices encode a node's xpus info to string
func EncodeNodeDevices(dlist []*types.DeviceInfo) string {
	var encodedNodeDevices strings.Builder
	for _, val := range dlist {
		encodedNodeDevices.Write([]byte(strconv.Itoa(int(val.Index))))
		encodedNodeDevices.Write([]byte(","))
		encodedNodeDevices.Write([]byte(val.Id))
		encodedNodeDevices.Write([]byte(","))
		encodedNodeDevices.Write([]byte(strconv.Itoa(int(val.Count))))
		encodedNodeDevices.Write([]byte(","))
		encodedNodeDevices.Write([]byte(strconv.Itoa(int(val.Devmem))))
		encodedNodeDevices.Write([]byte(","))
		encodedNodeDevices.Write([]byte(val.Type))
		encodedNodeDevices.Write([]byte(","))
		encodedNodeDevices.Write([]byte(strconv.FormatBool(val.Health)))
		encodedNodeDevices.Write([]byte(","))
		encodedNodeDevices.Write([]byte(strconv.Itoa(int(val.Numa))))
		encodedNodeDevices.Write([]byte(":"))
	}
	log.Infoln("Encoded node Devices:", encodedNodeDevices.String())
	return encodedNodeDevices.String()
}

// EncodeContainerDevices encode vxpu resource request of a container to string
func EncodeContainerDevices(cd types.ContainerDevices) string {
	var encodedContainerDevices strings.Builder
	for _, val := range cd {
		encodedContainerDevices.Write([]byte(strconv.Itoa(int(val.Index))))
		encodedContainerDevices.Write([]byte(","))
		encodedContainerDevices.Write([]byte(val.UUID))
		encodedContainerDevices.Write([]byte(","))
		encodedContainerDevices.Write([]byte(val.Type))
		encodedContainerDevices.Write([]byte(","))
		encodedContainerDevices.Write([]byte(strconv.Itoa(int(val.Usedmem))))
		encodedContainerDevices.Write([]byte(","))
		encodedContainerDevices.Write([]byte(strconv.Itoa(int(val.Usedcores))))
		encodedContainerDevices.Write([]byte(","))
		encodedContainerDevices.Write([]byte(strconv.Itoa(int(val.Vid))))
		encodedContainerDevices.Write([]byte(":"))
	}
	log.Infoln("Encoded container Devices:", encodedContainerDevices.String())
	return encodedContainerDevices.String()
}

// EncodePodDevices encode vxpu resource request of a pod to string
func EncodePodDevices(pd types.PodDevices) string {
	var ss []string
	for _, cd := range pd {
		ss = append(ss, EncodeContainerDevices(cd))
	}
	return strings.Join(ss, ",")
}

// GetXPUDevice get XPUDevice info
func GetXPUDevice(str string, ip string) map[string]*types.XPUDevice {
	deviceMap := DecodeNodeDevices(str)
	driverVersion, FrameworkVersion, err := xpu.GetVersionInfo()
	if err != nil {
		log.Infof("GetVersionInfo error %v", err)
	}
	for _, device := range deviceMap {
		device.NodeIp = ip
		device.NodeName = config.NodeName
		device.DriverVersion = driverVersion
		device.FrameworkVersion = FrameworkVersion
	}
	return deviceMap
}

// DecodeNodeDevices decode the node device from string
func DecodeNodeDevices(str string) map[string]*types.XPUDevice {
	deviceMap := make(map[string]*types.XPUDevice)
	if !strings.Contains(str, ":") {
		log.Errorf("decode node device failed, wrong annos: %s", str)
		return deviceMap
	}
	tmp := strings.Split(str, ":")
	for _, val := range tmp {
		if !strings.Contains(val, ",") {
			continue
		}
		items := strings.Split(val, ",")
		if len(items) != deviceLength {
			log.Warningf("device string is wrong, device: %s", items)
			continue
		}
		index, err := strconv.Atoi(items[0])
		if err != nil {
			continue
		}
		count, err := strconv.Atoi(items[2])
		if err != nil {
			continue
		}
		devmem, err := strconv.Atoi(items[3])
		if err != nil {
			continue
		}
		health, err := strconv.ParseBool(items[5])
		if err != nil {
			continue
		}
		i := types.XPUDevice{
			Index:          int32(index),
			Id:             items[1],
			Type:           items[4],
			Count:          uint32(count),
			MemoryTotal:    uint64(devmem),
			Health:         health,
			VxpuDeviceList: types.VxpuDevices{},
		}
		deviceMap[items[1]] = &i
	}
	return deviceMap
}

// DecodeContainerDevices decode xpu resource request of a container from string
func DecodeContainerDevices(str string) types.ContainerDevices {
	if len(str) == 0 {
		return types.ContainerDevices{}
	}
	cd := strings.Split(str, ":")
	contdev := types.ContainerDevices{}
	for _, val := range cd {
		if strings.Contains(val, ",") == false {
			continue
		}
		fields := strings.Split(val, ",")
		tmpdev := types.ContainerDevice{}
		if len(fields) != reflect.TypeOf(tmpdev).NumField() {
			log.Fatalln("DecodeContainerDevices invalid parameter:", str)
			return types.ContainerDevices{}
		}
		index, err := strconv.Atoi(fields[0])
		if err != nil {
			log.Fatalln("DecodeContainerDevices invalid parameter:", str)
			return types.ContainerDevices{}
		}
		tmpdev.Index = int32(index)
		tmpdev.UUID = fields[1]
		tmpdev.Type = fields[2]
		devmem, err := strconv.Atoi(fields[3])
		if err != nil {
			log.Fatalln("DecodeContainerDevices invalid parameter:", str)
			return types.ContainerDevices{}
		}
		tmpdev.Usedmem = int32(devmem)
		devcores, err := strconv.Atoi(fields[4])
		if err != nil {
			log.Fatalln("DecodeContainerDevices invalid parameter:", str)
			return types.ContainerDevices{}
		}
		tmpdev.Usedcores = int32(devcores)
		vid, err := strconv.Atoi(fields[5])
		if err != nil {
			log.Fatalln("DecodeContainerDevices invalid parameter:", str)
			return types.ContainerDevices{}
		}
		tmpdev.Vid = int32(vid)
		contdev = append(contdev, tmpdev)
	}
	return contdev
}

// DecodePodDevices decode xpu resource request of a pod from string
func DecodePodDevices(str string) types.PodDevices {
	if len(str) == 0 {
		return types.PodDevices{}
	}
	var pd types.PodDevices
	for _, s := range strings.Split(str, ";") {
		cd := DecodeContainerDevices(s)
		pd = append(pd, cd)
	}
	return pd
}

func getContainerIdxByVxpuIdx(p *v1.Pod, vxpuIdx int) int {
	foundVxpuIdx := -1
	for i, container := range p.Spec.Containers {
		_, ok := container.Resources.Limits[xpu.VxpuNumber]
		if ok {
			foundVxpuIdx++
			if foundVxpuIdx == vxpuIdx {
				return i
			}
			continue
		}
		_, ok = container.Resources.Limits[xpu.VxpuMemory]
		if ok {
			foundVxpuIdx++
			if foundVxpuIdx == vxpuIdx {
				return i
			}
			continue
		}
	}
	return -1
}

// Get xvpu limit info of the container
func getVxpuLimit(resourceList v1.ResourceList) (int64, int64, int64) {
	var number int64 = 0
	var core int64 = 0
	var mem int64 = 0

	if vxpuNumber, ok := resourceList[xpu.VxpuNumber]; ok {
		number = vxpuNumber.Value()
	}
	if vxpuCore, ok := resourceList[xpu.VxpuCore]; ok {
		core = vxpuCore.Value()
	}
	if vxpuMem, ok := resourceList[xpu.VxpuMemory]; ok {
		mem = vxpuMem.Value()
	}
	return number, core, mem
}

// GetNextDeviceRequest get next xpu resource request of container in a pod
// reference code: https://gitee.com/openeuler/kubernetes/blob/master/pkg/scheduler/app/plugins/deviceplugin/gpu/util.go
func GetNextDeviceRequest(dtype string, p v1.Pod) (v1.Container, types.ContainerDevices, error) {
	pdevices := DecodePodDevices(p.Annotations[xpu.AssignedIDsToAllocate])
	res := types.ContainerDevices{}
	for vxpuIdx, val := range pdevices {
		found := false
		for _, dev := range val {
			if strings.Compare(dtype, dev.Type) == 0 {
				res = append(res, dev)
				found = true
			}
		}
		if found {
			idx := getContainerIdxByVxpuIdx(&p, vxpuIdx)
			if idx != -1 {
				return p.Spec.Containers[idx], res, nil
			} else {
				log.Errorf("get container idx by vxpuIdx failed, vxpuIdx: %d", vxpuIdx)
			}
			break
		}
	}
	return v1.Container{}, res, errors.New("device request not found")
}

// EraseNextDeviceTypeFromAnnotation erase next xpu resource request of container in a pod's annotation
func EraseNextDeviceTypeFromAnnotation(dtype string, p v1.Pod) error {
	pdevices := DecodePodDevices(p.Annotations[xpu.AssignedIDsToAllocate])
	res := types.PodDevices{}
	found := false
	for _, val := range pdevices {
		if found {
			res = append(res, val)
			continue
		}
		tmp := types.ContainerDevices{}
		for _, dev := range val {
			if strings.Compare(dtype, dev.Type) == 0 {
				found = true
			} else {
				tmp = append(tmp, dev)
			}
		}
		if !found {
			res = append(res, val)
		} else {
			res = append(res, tmp)
		}
	}
	log.Infoln("After erase res=", res)
	newannos := make(map[string]string)
	newannos[xpu.AssignedIDsToAllocate] = EncodePodDevices(res)
	return PatchPodAnnotations(&p, newannos)
}

// PodAllocationSuccess try to patch annotation of a pod to indicate allocation success
func PodAllocationTrySuccess(nodeName string, pod *v1.Pod) {
	refreshed, _ := lock.GetClient().CoreV1().Pods(pod.Namespace).Get(context.Background(), pod.Name, metav1.GetOptions{})

	annos := refreshed.Annotations[xpu.AssignedIDsToAllocate]
	log.Infoln("TrySuccess:", annos)

	if strings.Contains(annos, xpu.DeviceType) {
		return
	}

	log.Infoln("AllDevicesAllocateSuccess releasing lock")
	PodAllocationSuccess(nodeName, pod)
}

// PodAllocationSuccess patch annotation of a pod to indicate allocation success
func PodAllocationSuccess(nodeName string, pod *v1.Pod) {
	newannos := make(map[string]string)
	newannos[types.DeviceBindPhase] = types.DeviceBindSuccess
	err := PatchPodAnnotations(pod, newannos)
	if err != nil {
		log.Errorln("patchPodAnnotations failed:%v", err.Error())
	}
	err = lock.ReleaseNodeLock(nodeName, types.VXPULockName)
	if err != nil {
		log.Errorf("release lock failed:%v", err.Error())
	}
}

// PodAllocationFailed patch annotation of a pod to indicate allocation failed
func PodAllocationFailed(nodeName string, pod *v1.Pod) {
	newannos := make(map[string]string)
	newannos[types.DeviceBindPhase] = types.DeviceBindFailed
	err := PatchPodAnnotations(pod, newannos)
	if err != nil {
		log.Errorln("patchPodAnnotations failed:%v", err.Error())
	}
	err = lock.ReleaseNodeLock(nodeName, types.VXPULockName)
	if err != nil {
		log.Errorf("release lock failed:%v", err.Error())
	}
}

// PatchNodeAnnotations patch annotation of a node
func PatchNodeAnnotations(nodeName string, annotations map[string]string) error {
	type patchMetadata struct {
		Annotations map[string]string `json:"annotations,omitempty"`
	}
	type patchPod struct {
		Metadata patchMetadata `json:"metadata"`
	}
	p := patchPod{}
	p.Metadata.Annotations = annotations
	bytes, err := json.Marshal(p)
	if err != nil {
		return err
	}
	_, err = lock.GetClient().CoreV1().Nodes().Patch(
		context.Background(),
		nodeName,
		k8stypes.StrategicMergePatchType,
		bytes,
		metav1.PatchOptions{})
	if err != nil {
		log.Infof("patch node %s failed, %v", nodeName, err)
	}
	return err
}

// PatchPodAnnotations patch annotation of a pod
func PatchPodAnnotations(pod *v1.Pod, annotations map[string]string) error {
	type patchMetadata struct {
		Annotations map[string]string `json:"annotations,omitempty"`
	}
	type patchPod struct {
		Metadata patchMetadata `json:"metadata"`
	}
	p := patchPod{}
	p.Metadata.Annotations = annotations
	bytes, err := json.Marshal(p)
	if err != nil {
		return err
	}
	_, err = lock.GetClient().CoreV1().Pods(pod.Namespace).Patch(
		context.Background(),
		pod.Name,
		k8stypes.StrategicMergePatchType,
		bytes,
		metav1.PatchOptions{})
	if err != nil {
		log.Infof("patch pod %v failed: %v", pod.Name, err)
	}
	return err
}

// GetXpus description get xpu info on the node
func GetXPUs() (map[string]*types.XPUDevice, error) {
	node, err := GetNode(config.NodeName)
	if err != nil {
		return nil, err
	}
	annos, ok := node.ObjectMeta.Annotations[xpu.NodeVXPURegister]
	if !ok {
		errMsg := fmt.Sprintf("node %s annotation %s is not exists",
			config.NodeName, xpu.NodeVXPURegister)
		log.Errorf(errMsg)
		return nil, errors.New(errMsg)
	}
	ip := getNodeIp(node)
	return GetXPUDevice(annos, ip), nil
}

func getNodeIp(node *v1.Node) string {
	for _, addr := range node.Status.Addresses {
		if addr.Type == v1.NodeInternalIP {
			return addr.Address
		}
	}
	log.Infoln("no expected node ip")
	return ""
}

// GetVgpus get all the xpu device info of the node
func GetVxpus() (types.VxpuDevices, map[string][]uint32, error) {
	selector := fields.SelectorFromSet(fields.Set{
		"spec.nodeName": config.NodeName,
		"status.phase":  string(v1.PodRunning),
	})
	podList, err := ListPods(metav1.ListOptions{
		FieldSelector: selector.String(),
	})
	if err != nil {
		log.Errorf("get pods in current node error: %v", err)
		return nil, nil, err
	}
	res := types.VxpuDevices{}
	pSet := make(map[string][]uint32)
	for _, pod := range podList.Items {
		if pod.Status.Phase != v1.PodRunning || len(pod.Status.ContainerStatuses) == 0 {
			errMsg := fmt.Sprintf(
				"pod status error: %v, container status len: %d",
				pod.Status.Phase, len(pod.Status.ContainerStatuses))
			log.Errorf(errMsg)
			continue
		}
		pdevices := DecodePodDevices(pod.Annotations[xpu.AssignedIDs])
		pi := 0
		for _, cs := range pod.Spec.Containers {
			number, core, mem := getVxpuLimit(cs.Resources.Limits)
			// If the container has not configured vxpu number
			// it means that the container has no vxpu.
			if number == 0 {
				continue
			}
			// check the length of pdevices
			if pi >= len(pdevices) {
				log.Warningf("pod %v does not have enough xpu devices for %v", pod.UID, pi)
				break
			}
			// The vxpu number should be equal to the length of types.ContainerDevices.
			if len(pdevices[pi]) != int(number) {
				log.Warningf("vxpu assigned info error, pod uid: %v, container name: %s", pod.UID, cs.Name)
				continue
			}
			for i := 0; i < int(number); i++ {
				dev := types.VxpuDevice{
					Id:              fmt.Sprintf("%s-%d", pdevices[pi][i].UUID, pdevices[pi][i].Vid),
					GpuId:           pdevices[pi][i].UUID,
					PodUID:          string(pod.UID),
					ContainerName:   cs.Name,
					VxpuMemoryLimit: mem * 1024,
					VxpuCoreLimit:   core,
				}
				res = append(res, dev)
			}
			pi += 1
			key := fmt.Sprintf("%s/%s", string(pod.UID), cs.Name)
			pSet[key] = []uint32{}
		}
	}
	return res, pSet, nil
}
