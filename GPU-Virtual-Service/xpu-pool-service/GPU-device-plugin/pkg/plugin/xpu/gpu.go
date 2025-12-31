//go:build vgpu

/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

// Package xpu defines and implements device abstraction layer
package xpu

import (
	"bufio"
	"bytes"
	"errors"
	"fmt"
	"io"
	"os/exec"
	"regexp"
	"strconv"
	"strings"
	"time"

	"k8s.io/kubelet/pkg/apis/deviceplugin/v1beta1"

	"huawei.com/vxpu-device-plugin/pkg/gonvml"
	"huawei.com/vxpu-device-plugin/pkg/graph"
	"huawei.com/vxpu-device-plugin/pkg/log"
	"huawei.com/vxpu-device-plugin/pkg/plugin/config"
	"huawei.com/vxpu-device-plugin/pkg/plugin/types"
)

const (
	// VxpuNumber vxpu number resource name
	VxpuNumber = "huawei.com/vgpu-number"
	// VxpuCore vxpu core resource name
	VxpuCore = "huawei.com/vgpu-cores"
	// VxpuMemory vxpu memory resource name
	VxpuMemory                      = "huawei.com/vgpu-memory.1Gi"
	microSecond                     = 1000 * 1000
	milliwatts                      = 1000
	eventWaitTimeout                = 5000
	nvidiaXidErrorPageFault         = 31
	nvidiaXidErrorStoppedProcessing = 43
	nvidiaXidErrorPreemptiveCleanup = 45
	// VisibleDevices visible nvidia devices env
	VisibleDevices = "NVIDIA_VISIBLE_DEVICES"
	// VxpuConfigFileName vxpu config file name
	VxpuConfigFileName = "vgpu.config"
	// VxpuConfigFileName vxpu ids config file name
	VxpuIdsConfigFileName = "vgpu-ids.config"
	// DeviceAssign device type supported by the device plugin
	DeviceType            = "GPU"
	AssignedIDs           = "huawei.com/vgpu-ids-new"
	AssignedIDsToAllocate = "huawei.com/vgpu-devices-to-allocate"
	NodeVXPUHandshake     = "huawei.com/node-vgpu-handshake"
	NodeVXPURegister      = "huawei.com/node-vgpu-register"
	NodeVXPUUsed          = "huawei.com/node-vgpu-used"
	// AssignedNode assigned node name
	AssignedNode = "huawei.com/vgpu-node"
	// NodeXpuTopology node gpu topology
	NodeXpuTopology = "huawei.com/node-gpu-topology"
)

var (
	// DevShmMount /dev/shm/ mount instance
	DevShmMount *v1beta1.Mount = nil
)

// Init initialize gpu nvml
func Init() error {
	log.Infoln("Loading NVML...")
	if ret := gonvml.Init(); ret != gonvml.Success {
		log.Infof("If this is a GPU node, did you set the docker default runtime to nvidia?")
		log.Infof("If this is not a GPU node, you should not deploy device plugin on this node.")
		return fmt.Errorf("failed to init NVML: %v", ret)
	}
	log.Infoln("NVML initialized successfully.")
	return nil
}

// Uninit uninitialize gpu nvml
func Uninit() error {
	ret := gonvml.Shutdown()
	log.Infof("NVML shutdown of returned: %v", ret)
	return nil
}

// DeviceManager implements the IDeviceManager interface for GPU devices on NVidia devices
type DeviceManager struct{}

func check(ret gonvml.NvmlRetType) {
	if ret != gonvml.Success {
		log.Panicln("Fatal:", ret)
	}
}

// Devices returns a list of Devices from the DeviceManager
func (*DeviceManager) Devices() []*Device {
	cnt, ret := gonvml.DeviceGetCount()
	check(ret)

	var devs []*Device
	for i := 0; i < cnt; i++ {
		dev, ret := gonvml.DeviceGetHandleByIndex(i)
		check(ret)
		devs = append(devs, buildDevice(dev, int32(i)))
	}
	return devs
}

func (*DeviceManager) CheckHealth(stop <-chan interface{}, devices []*Device, unhealthy chan<- *Device) {
	checkHealth(stop, devices, unhealthy)
}
func buildDevice(d gonvml.Device, logicID int32) *Device {
	dev := Device{}
	uuid, ret := d.GetUUID()
	check(ret)
	dev.ID = uuid
	dev.Health = v1beta1.Healthy
	dev.LogicID = logicID
	return &dev
}

// CheckHealth performs health checks on a set of devices, writing to the 'unhealthy' channel with any unhealthy devices
func checkHealth(stop <-chan interface{}, devices []*Device, unhealthy chan<- *Device) {
	eventSet, ret := gonvml.EventSetCreate()
	check(ret)
	defer gonvml.EventSetFree(eventSet)

	for _, d := range devices {
		ndev, ret := gonvml.DeviceGetHandleByUUID(d.ID)
		check(ret)
		// Register event for critical error
		ret = gonvml.DeviceRegisterEvents(ndev, gonvml.EventTypeXidCriticalError, eventSet)
		if ret != gonvml.Success {
			log.Warningf("Warning: register event for health check failed, mark it unhealthy. deviceId: %s, ret: %v", d.ID, ret)
			unhealthy <- d
			continue
		}
	}

	for {
		select {
		case <-stop:
			return
		default:
		}
		ed, ret := gonvml.EventSetWait(eventSet, eventWaitTimeout)
		if ret != gonvml.Success || ed.EventType != gonvml.EventTypeXidCriticalError {
			continue
		}
		// TODO: ME: formalize the full list and document it.
		//  Add events that should still be healthy
		if ed.EventData == nvidiaXidErrorPageFault ||
			ed.EventData == nvidiaXidErrorStoppedProcessing ||
			ed.EventData == nvidiaXidErrorPreemptiveCleanup {
			continue
		}
		uuid, ret := ed.Device.GetUUID()
		check(ret)
		if len(uuid) == 0 {
			log.Warningf("uuidCriticalError: Xid=%d, All devices will go unhealthy.", ed.EventData)
			for _, d := range devices {
				unhealthy <- d
			}
			continue
		}
		for _, d := range devices {
			if d.ID == uuid {
				log.Warningf("XidCriticalError: Xid=%d on Device=%s, the device will go unhealthy.", ed.EventData, d.ID)
				unhealthy <- d
				break
			}
		}
	}
}

// GetDeviceInfo create types.DeviceInfo according to Device
func GetDeviceInfo(devs []*Device) []*types.DeviceInfo {
	res := make([]*types.DeviceInfo, 0, len(devs))
	for _, dev := range devs {
		ndev, ret := gonvml.DeviceGetHandleByUUID(dev.ID)
		if ret != gonvml.Success {
			log.Fatalln("get device handle failed")
		}
		memInfo, ret := ndev.GetMemoryInfoV2()
		if ret != gonvml.Success {
			log.Fatalln("get memory info failed")
		}
		name, ret := ndev.GetName()
		if ret != gonvml.Success {
			log.Fatalln("get name failed")
		}
		numa, err := getNumaInformation(int(dev.LogicID))
		if err != nil {
			log.Warningf("get numa information for device %d failed: %s", dev.LogicID, err)
		}
		registeredMem := int32(memInfo.Total / 1024 / 1024)
		log.Infof("nvml registered deviceId", dev.ID, "memory", registeredMem, "name", name)
		res = append(res, &types.DeviceInfo{
			Index:  dev.LogicID,
			Id:     dev.ID,
			Count:  int32(config.DeviceSplitCount),
			Devmem: registeredMem,
			Type:   fmt.Sprintf("%v-%v", DeviceType, resolveDeviceName(name)),
			Health: dev.Health == v1beta1.Healthy,
			Numa:   int32(numa),
		})
	}
	return res
}

// resolveDeviceName resolve device name to abbreviations
// example "Tesla V100-PCIE-32GB" resolve to "V100"
func resolveDeviceName(deviceName string) string {
	if len(config.GPUTypeMap) != 0 {
		abbreviation, ok := config.GPUTypeMap[deviceName]
		if ok {
			log.Infof("find abbreviation from gpu type map, deviceName: %s, abbreviation: %s",
				deviceName, abbreviation)
			return abbreviation
		}
	}
	pattern := `^[A-Z]+[0-9]+[A-Z]*$`
	regex, err := regexp.Compile(pattern)
	if err != nil {
		log.Fatalln("regexp compile failed:", err)
		return strings.ReplaceAll(deviceName, " ", "")
	}
	nameSlice := strings.Split(strings.ReplaceAll(deviceName, " ", "-"), "-")
	for _, val := range nameSlice {
		if regex.MatchString(val) {
			return val
		}
	}
	return strings.ReplaceAll(deviceName, " ", "")
}

// GetVisibleDevices get visible devices for container env
func GetVisibleDevices(devReq types.ContainerDevices) string {
	visibleDevices := make([]string, 0)
	for _, dev := range devReq {
		visibleDevices = append(visibleDevices, dev.UUID)
	}
	return strings.Join(visibleDevices, ",")
}

// GetDeviceUsage get all gpu process usage
func GetXPUUsage(index, period int32) (types.DeviceUsageInfo, map[uint32]*types.ProcessUsage, error) {
	processMap := make(map[uint32]*types.ProcessUsage)
	dev, ret := gonvml.DeviceGetHandleByIndex(int(index))
	if ret != gonvml.Success {
		log.Errorf("gonvml.DeviceGetHandleByIndex error: %v", ret)
		return types.DeviceUsageInfo{}, nil, fmt.Errorf("gonvml.DeviceGetHandleByIndex failed: %v", ret)
	}
	retDeviceUsageInfo, err := getDeviceUsageInfo(dev)
	if err != nil {
		log.Errorf("get device usage info failed: %v", err)
		return types.DeviceUsageInfo{}, nil, fmt.Errorf("getDeviceUsageInfo failed: %v", err)
	}
	// The default length of the array is 1624, the default position without data are filled with 0.
	infos, ret := dev.GetComputeRunningProcesses()
	if ret != gonvml.Success && ret != gonvml.ErrorNotFound {
		log.Errorf("device GetComputeRunningProcesses failed: %v", ret)
		return types.DeviceUsageInfo{}, nil, fmt.Errorf("gonvml.GetComputeRunningProcesses failed: %v", ret)
	}
	// The default length of the array is 1624, the default position without data are filled with 0.
	timestamp := uint64(time.Now().Unix() - int64(period*microSecond))
	// Get the process utilization of different processes.
	samples, ret := dev.DeviceGetProcessUtilization(timestamp)
	if ret != gonvml.Success && ret != gonvml.ErrorNotFound {
		log.Errorf("device GetProcessUtilization failed: %v", ret)
		return types.DeviceUsageInfo{}, nil, fmt.Errorf("gonvml.DeviceGetProcessUtilization failed: %v", ret)
	}
	// To prevent two info from corresponding to different processes.
	for _, v := range infos {
		if v.Pid == 0 {
			break
		}
		p := types.ProcessUsage{ProcessMem: v.UsedGpuMemory, ProcessCoreUtilization: 0}
		processMap[v.Pid] = &p
	}
	for _, v := range samples {
		if v.Pid == 0 {
			// if vgpuID is 0, it means the data has ended, break.
			break
		}
		if _, ok := processMap[v.Pid]; !ok {
			p := types.ProcessUsage{ProcessMem: 0, ProcessCoreUtilization: 0}
			processMap[v.Pid] = &p
		}
		processMap[v.Pid].ProcessCoreUtilization = uint64(v.SmUtil)
	}
	return retDeviceUsageInfo, processMap, nil
}

func getDeviceUsageInfo(dev gonvml.Device) (types.DeviceUsageInfo, error) {
	utilization, ret := dev.GetUtilizationRates()
	if ret != gonvml.Success && ret != gonvml.ErrorNotFound {
		log.Errorf("gonvml.GetUtilizationRates failed: %v", ret)
		return types.DeviceUsageInfo{}, fmt.Errorf("gonvml.GetUtilizationRates failed: %v", ret)
	}
	powerUsage, ret := dev.GetPowerUsage()
	if ret != gonvml.Success && ret != gonvml.ErrorNotFound {
		log.Errorf("device GetPowerUsage failed: %v", ret)
		return types.DeviceUsageInfo{}, fmt.Errorf("gonvml.GetPowerUsage failed: %v", ret)
	}
	temperature, ret := dev.GetTemperature(gonvml.NvmlTemperatureGpu)
	if ret != gonvml.Success && ret != gonvml.ErrorNotFound {
		log.Errorf("device GetTemperature failed: %v", ret)
		return types.DeviceUsageInfo{}, fmt.Errorf("gonvml.GetTemperature failed: %v", ret)
	}
	deviceUsageInfo := types.DeviceUsageInfo{
		CoreUtil:    utilization.Gpu,
		MemUtil:     utilization.Memory,
		PowerUsage:  powerUsage / milliwatts,
		Temperature: temperature,
	}
	return deviceUsageInfo, nil
}

const (
	// defaultNvidiaSmiBinary default nvidia-smi executable path.
	defaultNvidiaSmiBinary = "/usr/bin/nvidia-smi"
	// nvidiaSmiCommand means the nvidia-smi command.
	nvidiaSmiExecutable = "nvidia-smi"
	// notapplicable means no numa for the specified GPU.
	notApplicable = "N/A"
)

var (
	gpuRegexp = regexp.MustCompile(`GPU (\d+)`)
	// gpuRegexp matches a GPU device e.g. GPU 0, GPU 01 etc.
	nvRegexp = regexp.MustCompile(`NV(\d+)`)
	// nvRegexp matches NVLinks between devices e.g. NV1, NV2 etc.
	splitter = regexp.MustCompile("[ \t]+")
	// splitter is a regex to split command output into separate tokens.
)

// gpuTopologyProvider is a gpu topology provider implementation.
type gpuTopologyProvider struct{}

var _ graph.TopologyProvider = (*gpuTopologyProvider)(nil)

// NewTopologyProvider creates an TopologyProvider instance.
func NewTopologyProvider() graph.TopologyProvider {
	return &gpuTopologyProvider{}
}

func (provider *gpuTopologyProvider) Topology() string {
	graph, err := provider.buildTopologyGraph()
	if err != nil {
		log.Errorf("build gpu topology error: %s", err)
		return ""
	}
	return graph.GetTopologyGraph()
}

// buildTopologyGraph builds topology graph for gpu.
// Currently, we get the GPU topology by parsing output of nvidia-smi output.
func (provider *gpuTopologyProvider) buildTopologyGraph() (graph.TopologyGraph, error) {
	stdOut, err := getGpuTopologyFromCommand()
	if err != nil {
		return nil, err
	}
	return parseTopologyGraph(stdOut)
}

// getTopologyFromCommand get topology output of command "nvidia-smi topo --matrix".
func getGpuTopologyFromCommand() (*bytes.Buffer, error) {
	stdout := new(bytes.Buffer)
	cmd := exec.Command(lookExecutableOrDefault(nvidiaSmiExecutable, defaultNvidiaSmiBinary), "topo", "--matrix")
	cmd.Stdout = stdout
	if err := cmd.Run(); err != nil {
		return nil, fmt.Errorf("execute %q failed: %w", cmd.String(), err)
	}
	return stdout, nil
}

// lookExecutableInPath looks for executable file from the PATH environment variables, and return default file is not found.
func lookExecutableOrDefault(file string, defaultFile string) string {
	binary, err := exec.LookPath(file)
	if err != nil {
		return defaultFile
	}
	return binary
}

// parseTopologyGraph parses the output of "nvidia-smi topo --matrix" command into a topology graph.
// Example output:
//
//	GPU0    GPU1    CPU Affinity    NUMA Affinity    GPU NUMA ID
//
// GPU0      X      PHB     0-23            N/A              N/A
// GPU1     PHB      X      0-23            N/A              N/A
// Legend:
func parseTopologyGraph(reader io.Reader) (graph.TopologyGraph, error) {
	scanner := bufio.NewScanner(reader)
	gpuCount := 0
	// handle header
	if scanner.Scan() {
		gpuCount = getGpuCountFromHeader(scanner.Text())
	}
	// this is header, handle begins at next line
	g := graph.NewTopologyGraph(gpuCount)
	i := 0
	for scanner.Scan() && i < gpuCount {
		text := scanner.Text()
		log.Debugf("parse topology graph line %d: %s", i, text)
		tokens := splitter.Split(strings.TrimSpace(text), -1)
		// tokens[0] is GPU identifier
		for j := 1; j <= gpuCount && j < len(tokens); j++ {
			if i == j-1 { // means GPU itself
				continue
			}
			g[i][j-1] = detectRate(i, j-1, tokens[j])
		}
		i++
	}
	return g, nil
}

// getGpuCountFromHeader counts how many GPUs on the host
// by parsing first line of nvidia-smi topo command.
// Header example:
//
//	GPU0    GPU1    CPU Affinity    NUMA Affinity    GPU NUMA ID
func getGpuCountFromHeader(header string) int {
	log.Debugf("get gpu count from header: %s", header)
	tokens := splitter.Split(strings.TrimSpace(header), -1)
	count := 0
	for _, s := range tokens {
		if gpuRegexp.MatchString(s) {
			count++
		}
	}
	return count
}

var (
	nvLinkBaseRate = 50 // for WLN link type, we give it a base rate 50
	nvLinkUnitRate = 10 // for each NVLink, it contributes extra rate to the base rate
	rate           = map[string]int{
		"PIX":  50,
		"PXB":  40,
		"PHB":  30,
		"NODE": 20,
		"SYS":  10,
	}
)

// detectRate finds the rate between the devices.
func detectRate(deviceId1 int, deviceId2 int, linkType string) int {
	matchNvLink := nvRegexp.FindStringSubmatch(linkType)
	if len(matchNvLink) == 0 { // not nvlink
		return rate[linkType]
	}

	// for group match nvRegexp, if matches, the result should contain original match string,
	// plus the group match string, so the result length must be 2.
	// index 1 means group match string, which is the number of NVlinks.
	n, err := strconv.ParseInt(matchNvLink[1], 10, 32)
	if err != nil {
		log.Errorf("parse nvlink failed: %s", err)
		return 0
	}
	// each nvlink contribute nvlink unit rate to teh nv link base rate
	return nvLinkBaseRate + nvLinkUnitRate*int(n)
}

// getGpuNumaInformation return numa information by provided card index.
func getNumaInformation(index int) (int, error) {
	reader, err := getGpuTopologyFromCommand()
	if err != nil {
		return 0, err
	}
	return parseNvidiaNumaInfo(index, reader)
}

// parseNvidiaNumaInfo parse gpu numa for the GPU with provided index.
func parseNvidiaNumaInfo(index int, reader io.Reader) (int, error) {
	scanner := bufio.NewScanner(reader)
	numaAffinityColumnIndex := 0
	// handle header
	if scanner.Scan() {
		numaAffinityColumnIndex = getNumaAffinityColumnIndex(scanner.Text())
	}
	target := fmt.Sprintf("GPU%d", index)
	for scanner.Scan() {
		tokens := strings.Split(strings.ReplaceAll(scanner.Text(), "\t\t", "\t"), "\t")
		if !strings.Contains(tokens[0], target) {
			continue
		}
		log.Debugf("topology row of GPU%d: tokens: %s, length: %d", index, tokens, len(tokens))
		if numaAffinityColumnIndex < len(tokens) {
			if tokens[numaAffinityColumnIndex] == notApplicable {
				log.Debugf("current card %d has not established numa topology", index)
				return 0, nil
			}
			return strconv.Atoi(tokens[numaAffinityColumnIndex])
		}
	}
	return 0, nil
}

// getNumaAffinityColumnIndex get the index of "NUMA Affinity" from the topology header.
func getNumaAffinityColumnIndex(header string) int {
	index := 0
	tokens := strings.Split(strings.ReplaceAll(header, "\t\t", "\t"), "\t")
	// The topology header is as follows
	// GPU0    GPU1    CPU Affinity    NUMA Affinity    GPU NUMA ID  <-- header
	// Legend: ...
	// The topology of a multiple cards is as follows
	// GPU0      X      PHB     0-23            N/A              N/A
	// GPU1     PHB      X      0-23            N/A              N/A
	// Legend: ...
	for idx, headerVal := range tokens {
		if strings.Contains(headerVal, "NUMA Affinity") {
			index = idx
			break
		}
	}
	log.Debugf("getNumaAffinityColumnIndex: tokens: %s, length: %d, index: %d", tokens, len(tokens), index)
	return index
}

// GetVersionInfo get version information
func GetVersionInfo() (string, int, error) {
	driverVersion, ret := gonvml.SystemGetDriverVersion()
	if ret != gonvml.Success {
		log.Errorf("get driver Version error: %v", ret)
		return "", 0, errors.New("get driver Version error")
	}
	cudaVersion, ret := gonvml.SystemGetCudaDriverVersion()
	if ret != gonvml.Success {
		log.Errorf("get cuda driver Version error: %v", ret)
		return driverVersion, 0, errors.New("get cuda Version error")
	}
	return driverVersion, cudaVersion, nil
}
