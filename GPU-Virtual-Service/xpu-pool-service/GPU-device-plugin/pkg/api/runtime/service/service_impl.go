/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

// Package service implements service of getting pids
package service

import (
	"bufio"
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"net"
	"os"
	"path/filepath"
	"regexp"
	"strconv"
	"strings"
	"syscall"
	"time"

	"google.golang.org/grpc"
	"huawei.com/vxpu-device-plugin/pkg/log"
	"huawei.com/vxpu-device-plugin/pkg/plugin/config"
	"huawei.com/vxpu-device-plugin/pkg/plugin/types"
	"huawei.com/vxpu-device-plugin/pkg/plugin/util"
	"huawei.com/vxpu-device-plugin/pkg/plugin/xpu"
	"k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/fields"
	"k8s.io/klog/v2"
)

const (
	cgroupBaseDir                 = "/sys/fs/cgroup/memory"
	cgroupProcs                   = "cgroup.procs"
	pidsSockPath                  = "/var/lib/xpu/pids.sock"
	hostProcDir                   = "/hostproc"
	procStatus                    = "status"
	nsPid                         = "NSpid:"
	nsPidFieldCount               = 3
	containerdPodIdPrefix         = "-pod"
	dockerPodIdPrefix             = "/pod"
	containerdPodIdSuffix         = ".slice"
	dockerPodIdSuffix             = "/"
	containerIdPrefix             = "cri-containerd-"
	containerIdSuffix             = ".scope"
	containerIdPrefixInContainerd = "containerd://"
	containerIdPrefixInDocker     = "docker://"
	vxpuConfigBaseDir             = "/etc/xpu"
	pidsConfigFileName            = "pids.config"
	configFilePerm                = 0644
	pidsSockPerm                  = 0666
	podDirCleanInterval           = 60
	minPeriod                     = 1
	maxPeriod                     = 86400
	defaultPeriod                 = 60
	percentage                    = 100
	float64BitsSize               = 64
)

// PidsServiceServerImpl implementation of pids service
type PidsServiceServerImpl struct {
	*UnimplementedPidsServiceServer
}

func readProcsFile(file string) ([]int, error) {
	f, err := os.Open(file)
	if err != nil {
		return nil, err
	}
	defer f.Close()

	scanner := bufio.NewScanner(f)
	pids := make([]int, 0)
	for scanner.Scan() {
		line := scanner.Text()
		if pid, err := strconv.Atoi(line); err == nil {
			pids = append(pids, pid)
		}
	}

	return pids, nil
}

func readStatusFile(file string) (string, error) {
	f, err := os.Open(file)
	if err != nil {
		return "", err
	}
	defer f.Close()

	scanner := bufio.NewScanner(f)
	for scanner.Scan() {
		line := scanner.Text()
		if strings.HasPrefix(line, nsPid) {
			eles := strings.Fields(line)
			if len(eles) != nsPidFieldCount {
				return "", errors.New("NpPid field count error")
			}
			pids := fmt.Sprintf("%-11s %-11s", eles[1], eles[2])
			return pids, nil
		}
	}
	return "", errors.New("NpPid not found")
}

func getPidMaps(hostPids []int) string {
	pidMaps := make([]string, 0)
	for _, hp := range hostPids {
		procStatusPath := filepath.Clean(filepath.Join(hostProcDir, strconv.Itoa(hp), procStatus))
		pids, err := readStatusFile(procStatusPath)
		if err != nil {
			klog.Warning("read proc status error: %v, path: %s", err, procStatusPath)
		} else {
			pidMaps = append(pidMaps, pids)
		}
	}
	return strings.Join(pidMaps, ",")
}

func parseCgroupPath(cgroupPath string) (string, string, error) {
	// 定义正则表达式模式
	podPattern := `([a-f0-9]{8}_[a-f0-9]{4}_[a-f0-9]{4}_[a-f0-9]{4}_[a-f0-9]{12})`
	containerPattern := `([a-f0-9]{64})`

	// 编译正则表达式
	podRegex := regexp.MustCompile(podPattern)
	containerRegex := regexp.MustCompile(containerPattern)

	// 查找podId
	podMatches := podRegex.FindStringSubmatch(cgroupPath)
	if len(podMatches) < 2 {
		return "", "", errors.New("pod id not found")
	}
	podId := podMatches[1]
	podId = strings.Replace(podId, "_", "-", -1)

	// 查找containerId
	containerMatches := containerRegex.FindStringSubmatch(cgroupPath)
	if len(containerMatches) < 2 {
		return "", "", errors.New("container id not found")
	}
	containerId := containerMatches[1]

	return podId, containerId, nil
}

func getContainerName(cgroupPath string) (string, string, error) {
	podId, containerId, err := parseCgroupPath(cgroupPath)
	log.Infof("podID: %s, containerId: %s", podId, containerId)
	if err != nil {
		klog.Error("parse cgroup path error: %v", err)
		return "", "", err
	}
	selector := fields.SelectorFromSet(fields.Set{"spec.nodeName": config.NodeName, "status.phase": string(v1.PodRunning)})
	podList, err := util.ListPods(
		metav1.ListOptions{
			FieldSelector: selector.String(),
		})
	if err != nil {
		return podId, "", err
	}
	for _, pod := range podList.Items {
		if string(pod.UID) != podId {
			continue
		}
		if pod.Status.Phase != v1.PodRunning || len(pod.Status.ContainerStatuses) == 0 {
			errMsg := fmt.Sprintf("pod status error: %v, container status len: %d",
				pod.Status.Phase, len(pod.Status.ContainerStatuses))
			return podId, "", errors.New(errMsg)
		}
		for _, cs := range pod.Status.ContainerStatuses {
			if cs.ContainerID[len(containerIdPrefixInContainerd):] != containerId &&
				cs.ContainerID[len(containerIdPrefixInDocker):] != containerId {
				continue
			}
			return podId, cs.Name, nil
		}
	}
	return podId, "", errors.New("container not found")
}

func readPidsConfig(pidsConfigPath string) ([]uint32, error) {
	f, err := os.OpenFile(pidsConfigPath, os.O_RDONLY, configFilePerm)
	if err != nil {
		return nil, err
	}
	defer f.Close()

	var pids []uint32
	scanner := bufio.NewScanner(f)
	for scanner.Scan() {
		line := scanner.Text()
		tmp := strings.Fields(line)
		if len(tmp) == 0 {
			continue
		}
		pid, err := strconv.Atoi(tmp[0])
		if err != nil {
			continue
		}
		pids = append(pids, uint32(pid))
	}
	return pids, nil
}

func writePidsConfig(pidsConfigPath, pidMaps string) error {
	f, err := os.OpenFile(pidsConfigPath, os.O_WRONLY|os.O_TRUNC|os.O_CREATE, configFilePerm)
	if err != nil {
		return err
	}
	defer f.Close()

	w := bufio.NewWriter(f)
	pids := strings.Split(pidMaps, ",")
	for _, pid := range pids {
		_, err = w.WriteString(pid + "\n")
		if err != nil {
			return err
		}
	}
	return w.Flush()
}

func getPodDirNames() ([]string, error) {
	dirNames := make([]string, 0)
	err := filepath.Walk(vxpuConfigBaseDir, func(path string, info os.FileInfo, err error) error {
		if err != nil {
			return err
		}
		if info == nil || !info.IsDir() || path == vxpuConfigBaseDir {
			return nil
		}
		dirNames = append(dirNames, info.Name())
		return filepath.SkipDir
	})
	if err != nil {
		return []string{}, err
	}
	return dirNames, nil
}

type void struct{}

var val void

func cleanDestroyedPodDir() error {
	podDirNames, err := getPodDirNames()
	if err != nil {
		return err
	}

	selector := fields.SelectorFromSet(fields.Set{"spec.nodeName": config.NodeName})
	podList, err := util.ListPods(
		metav1.ListOptions{
			FieldSelector: selector.String(),
		})
	if err != nil {
		return err
	}

	podIdSet := make(map[string]void)
	for _, pod := range podList.Items {
		podIdSet[string(pod.UID)] = val
	}
	for _, podDirName := range podDirNames {
		if _, ok := podIdSet[podDirName]; ok {
			continue
		}
		podAbsoluteDir := filepath.Clean(filepath.Join(vxpuConfigBaseDir, podDirName))
		err = os.RemoveAll(podAbsoluteDir)
	}
	return nil
}

// GetPids pids service external interface, get all pids map relationship in container
func (PidsServiceServerImpl) GetPids(ctx context.Context, req *GetPidsRequest) (*GetPidsResponse, error) {
	cgroupAbsolutePath := filepath.Clean(filepath.Join(cgroupBaseDir, req.CgroupPath, cgroupProcs))
	hostPids, err := readProcsFile(cgroupAbsolutePath)
	if err != nil {
		return nil, err
	}
	pidMaps := getPidMaps(hostPids)
	podId, containerName, err := getContainerName(req.CgroupPath)
	if err != nil {
		return nil, err
	}
	pidsConfigPath := filepath.Clean(filepath.Join(vxpuConfigBaseDir, podId, containerName, pidsConfigFileName))
	err = writePidsConfig(pidsConfigPath, pidMaps)
	if err != nil {
		return nil, err
	}
	return &GetPidsResponse{EncodedPids: pidMaps}, nil
}

func getPodSet(pSet map[string][]uint32) map[string][]uint32 {
	if pSet == nil {
		return nil
	}
	for k := range pSet {
		pidsConfigPath := filepath.Clean(filepath.Join(vxpuConfigBaseDir, k, pidsConfigFileName))
		pids, err := readPidsConfig(pidsConfigPath)
		if err != nil {
			continue
		}
		pSet[k] = pids
	}
	return pSet
}

func setVxpuDevices(vxpuDevices types.VxpuDevices,
	xpuDevices map[string]*types.XPUDevice,
	uidToProcessMap map[string]map[uint32]*types.ProcessUsage,
	pSet map[string][]uint32) map[string]*types.XPUDevice {
	for _, v := range vxpuDevices {
		if _, ok := xpuDevices[v.GpuId]; !ok {
			continue
		}

		processUsage, ok := uidToProcessMap[v.GpuId]
		if !ok {
			xpuDevices[v.GpuId].VxpuDeviceList = append(xpuDevices[v.GpuId].VxpuDeviceList, v)
			continue
		}

		pidkey := fmt.Sprintf("%s/%s", v.PodUID, v.ContainerName)
		for _, pid := range pSet[pidkey] {
			if pUsage, ok := processUsage[pid]; ok {
				v.VxpuCoreUtilization += float64(pUsage.ProcessCoreUtilization)
				v.VxpuMemoryUsed += pUsage.ProcessMem
			}
		}
		v.VxpuMemoryUsed = v.VxpuMemoryUsed / 1024 / 1024
		if xpuDevices[v.GpuId].MemoryTotal != 0 {
			vxpuMemoryUtil := float64(v.VxpuMemoryUsed*percentage) / float64(xpuDevices[v.GpuId].MemoryTotal)
			vxpuMemoryUtil, _ = strconv.ParseFloat(fmt.Sprintf("%.2f", vxpuMemoryUtil), float64BitsSize)
			v.VxpuMemoryUtilization = vxpuMemoryUtil
		}
		xpuDevices[v.GpuId].VxpuDeviceList = append(xpuDevices[v.GpuId].VxpuDeviceList, v)
	}
	for _, device := range xpuDevices {
		for _, vxpuDevices := range device.VxpuDeviceList {
			device.MemoryUsed += vxpuDevices.VxpuMemoryUsed
		}
		memoryUtil := float64(device.MemoryUsed*percentage) / float64(device.MemoryTotal)
		memoryUtil, _ = strconv.ParseFloat(fmt.Sprintf("%.2f", memoryUtil), float64BitsSize)
		device.MemoryUtilization = memoryUtil
	}
	return xpuDevices
}

// GetAllVgpuInfo get all vgpu info of the node
func (PidsServiceServerImpl) GetAllVxpuInfo(ctx context.Context, req *GetAllVxpuInfoRequest) (*GetAllVxpuInfoResponse, error) {
	period, err := strconv.Atoi(req.Period)
	if err != nil || period < minPeriod || period > maxPeriod {
		period = defaultPeriod
	}

	// xpuDevices: map[uuid]xpuDevice
	xpuDevices, err := util.GetXPUs()
	if err != nil {
		return nil, err
	}

	// vgpuDevices: types.VgpuDevices[]
	vxpuDevices, pSet, err := util.GetVxpus()
	if err != nil {
		return nil, err
	}

	// pSet: map["podId/containerName"]pids
	pSet = getPodSet(pSet)

	// uidToProcessMap: map[uuid]map[processId]processUsage
	uidToProcessMap := make(map[string]map[uint32]*types.ProcessUsage)
	for _, v := range xpuDevices {
		deviceUsageInfo, processMap, err := xpu.GetXPUUsage(v.Index, int32(period))
		if err != nil {
			return nil, err
		}
		// XpuUtilization = deviceUsageInfo.CoreUtil
		v.XpuUtilization = float64(deviceUsageInfo.CoreUtil)
		v.PowerUsage = deviceUsageInfo.PowerUsage
		v.Temperature = deviceUsageInfo.Temperature
		uidToProcessMap[v.Id] = processMap
	}
	xpuDevices = setVxpuDevices(vxpuDevices, xpuDevices, uidToProcessMap, pSet)
	jsonVgpuInfos, err := json.Marshal(xpuDevices)
	if err != nil {
		return nil, err
	}
	return &GetAllVxpuInfoResponse{VxpuInfos: string(jsonVgpuInfos)}, nil
}

// Start run pids service
func Start() {
	srv := grpc.NewServer()
	RegisterPidsServiceServer(srv, PidsServiceServerImpl{})
	err := syscall.Unlink(pidsSockPath)
	if err != nil && !os.IsNotExist(err) {
		return
	}
	listener, err := net.Listen("unix", pidsSockPath)
	if err != nil {
		return
	}
	err = os.Chmod(pidsSockPath, pidsSockPerm)
	if err != nil {
		return
	}
	go func() {
		err := srv.Serve(listener)
		if err != nil {
		}
	}()
	go func() {
		for {
			time.Sleep(time.Second * podDirCleanInterval)
			err := cleanDestroyedPodDir()
			if err != nil {
				break
			}
		}
	}()
}
