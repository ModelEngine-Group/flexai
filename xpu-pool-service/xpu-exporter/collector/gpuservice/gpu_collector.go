/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

//Package gpuservice for Prometheus

package gpuservice

import (
	"encoding/json"
	"reflect"
	"strconv"
	"time"

	"github.com/prometheus/client_golang/prometheus"
	"huawei.com/vxpu-device-plugin/pkg/log"

	"huawei.com/xpu-exporter/common/cache"
	"huawei.com/xpu-exporter/common/client"
	"huawei.com/xpu-exporter/common/utils"
	"huawei.com/xpu-exporter/versions"
)

const (
	podName       = "pod_name"
	cntrName      = "container_name"
	gpuUUid       = "gpu_uuid"
	podUid        = "pod_uuid"
	vgpuId        = "vgpu_id"
	vgpuCoreLimit = "vgpu_core_limit"
	vgpuMemLimit  = "vgpu_mem_limit"
	nodeName      = "node_name"
	nodeIp        = "node_ip"
	gpuNum        = "gpu_num"
	nvmlIndex     = "nvml_index"
	model         = "model"
	driverVersion = "driver_version"
	cudaVersion   = "cuda_version"
)

var (
	vgpuLabel = []string{gpuUUid, nodeName, nodeIp, podUid, cntrName, vgpuId, vgpuCoreLimit, vgpuMemLimit}
	gpuLabel  = []string{gpuUUid, nodeName, nodeIp, nvmlIndex, model, driverVersion, cudaVersion}
	nodeLabel = []string{nodeName, nodeIp}
)

var (
	versionInfoDesc = prometheus.NewDesc("gpu_exporter_version_info",
		"exporter version with value '1'", []string{"exporterVersion"}, nil)
	xpuGpuUtilizationDesc = prometheus.NewDesc("xpu_gpu_util",
		"the utilization rate of computing power for a single gpu", gpuLabel, nil)
	xpuGpuMemoryUtilizationDesc = prometheus.NewDesc("xpu_gpu_mem_util",
		"the utilization rate of memory for a single gpu", gpuLabel, nil)
	xpuGpuStatusDesc = prometheus.NewDesc("xpu_gpu_status",
		"gpu card health status.", gpuLabel, nil)
	xpuGpuNumberDesc = prometheus.NewDesc("xpu_gpu_num",
		"number of gpus", nodeLabel, nil)
	xpuGpuMemoryDesc = prometheus.NewDesc("xpu_gpu_mem",
		"memory size of gpu", gpuLabel, nil)
	xpuGpuPowerUsageDesc = prometheus.NewDesc("xpu_gpu_power_usage",
		"power usage of gpu, the unit is milliwatts", gpuLabel, nil)
	xpuGpuTemperatureDesc = prometheus.NewDesc("xpu_gpu_temperature",
		"temperature of gpu, the unit is Celsius", gpuLabel, nil)
	xpuVgpuUtilizationDesc = prometheus.NewDesc("xpu_vgpu_util",
		"the utilization rate of computing power for vgpu", vgpuLabel, nil)
	xpuVgpuMemoryUtilizationDesc = prometheus.NewDesc("xpu_vgpu_mem_util",
		"the utilization rate of memory for vgpu", vgpuLabel, nil)
	xpuVgpuNumberDesc = prometheus.NewDesc("xpu_vgpu_num",
		"real time quantity of vgpu", []string{nodeName, nodeIp, gpuUUid}, nil)
	xpuVgpuPodNumberDesc = prometheus.NewDesc("xpu_vgpu_pod_num",
		"real time quantity of vgpu pods", []string{nodeName, nodeIp, gpuUUid}, nil)

	descriptions = []*prometheus.Desc{versionInfoDesc, xpuGpuUtilizationDesc, xpuGpuMemoryUtilizationDesc,
		xpuGpuStatusDesc, xpuGpuNumberDesc, xpuGpuMemoryDesc, xpuGpuPowerUsageDesc, xpuGpuTemperatureDesc,
		xpuVgpuUtilizationDesc, xpuVgpuMemoryUtilizationDesc, xpuVgpuNumberDesc, xpuVgpuPodNumberDesc}
)

const (
	cacheSize = 128
)

type gpuCollector struct {
	cache      *cache.ConcurrencyLRUCache
	updateTime time.Duration
	cacheTime  time.Duration
}

// Describe implements prometheus.Collector
func (n *gpuCollector) Describe(ch chan<- *prometheus.Desc) {
	if ch == nil {
		log.Warningln("Invalid param in function Describe")
		return
	}
	for _, desc := range descriptions {
		ch <- desc
	}
}

// Collect implements prometheus.Collector
func (n *gpuCollector) Collect(ch chan<- prometheus.Metric) {
	if !validate(ch) {
		log.Warningln("Invalid param in function Collect")
		return
	}

	gpuDeviceMap := getVgpuInfoInCache(ch, n)
	ch <- prometheus.MustNewConstMetric(versionInfoDesc, prometheus.GaugeValue, 1,
		[]string{versions.BuildVersion}...)

	gpuDeviceCount := len(gpuDeviceMap)
	var vgpuDeviceTotalCount = 0
	var nodeName string
	var nodeIp string

	for _, gpuDevice := range gpuDeviceMap {
		nodeName = gpuDevice.NodeName
		nodeIp = gpuDevice.NodeIp
		updateGpuDeviceInfo(ch, gpuDevice)
		vgpuDeviceCount := len(gpuDevice.VxpuDeviceList)
		if vgpuDeviceCount <= 0 {
			continue
		}
		vgpuDeviceTotalCount += vgpuDeviceCount
		updateVgpuDeviceInfo(ch, gpuDevice)
	}
	ch <- prometheus.MustNewConstMetric(xpuGpuNumberDesc, prometheus.CounterValue, float64(gpuDeviceCount),
		[]string{nodeName, nodeIp}...)
}

func getVgpuInfoInCache(ch chan<- prometheus.Metric, n *gpuCollector) map[string]*utils.XPUDevice {
	if ch == nil {
		log.Errorln("metric channel is nil")
		return nil
	}

	obj, err := n.cache.Get(vgpuInfoCacheKey)
	if obj == nil {
		log.Warningln("no cache, start to get vgpuInfo and rebuild cache.")
		vgpuInfo, err := client.GetAllVxpuInfo()
		if err != nil {
			log.Errorf("get vgpuInfo error: %v", err)
			return nil
		}
		if err = n.cache.Set(vgpuInfoCacheKey, vgpuInfo, n.cacheTime); err != nil {
			log.Errorf("no cache for prometheus, try to build cache failed, error is: %v", err)
		} else {
			log.Infoln("rebuild cache successfully")
		}
		obj = vgpuInfo
	}

	var gpuDeviceMap map[string]*utils.XPUDevice
	err = json.Unmarshal([]byte(obj.(string)), &gpuDeviceMap)
	if err != nil {
		log.Errorf("Error vgpu info cache and convert failed: %v", err)
	}
	return gpuDeviceMap
}

func updateGpuDeviceInfo(ch chan<- prometheus.Metric, gpu *utils.XPUDevice) {
	if !validate(ch) {
		log.Warningln("Invalid param in function updateGpuDeviceInfo")
		return
	}
	ch <- prometheus.MustNewConstMetric(xpuGpuUtilizationDesc, prometheus.GaugeValue, gpu.XpuUtilization,
		[]string{gpu.Id, gpu.NodeName, gpu.NodeIp, strconv.Itoa(int(gpu.Index)), gpu.Type, gpu.DriverVersion,
			strconv.Itoa(gpu.FrameworkVersion)}...)
	ch <- prometheus.MustNewConstMetric(xpuGpuMemoryUtilizationDesc, prometheus.GaugeValue,
		gpu.MemoryUtilization, []string{gpu.Id, gpu.NodeName, gpu.NodeIp, strconv.Itoa(int(gpu.Index)),
			gpu.Type, gpu.DriverVersion, strconv.Itoa(gpu.FrameworkVersion)}...)
	var gpuStatus = 0
	if gpu.Health {
		gpuStatus = 1
	}
	ch <- prometheus.MustNewConstMetric(xpuGpuStatusDesc, prometheus.GaugeValue, float64(gpuStatus),
		[]string{gpu.Id, gpu.NodeName, gpu.NodeIp, strconv.Itoa(int(gpu.Index)), gpu.Type, gpu.DriverVersion,
			strconv.Itoa(gpu.FrameworkVersion)}...)
	ch <- prometheus.MustNewConstMetric(xpuGpuMemoryDesc, prometheus.GaugeValue,
		float64(gpu.MemoryTotal), []string{gpu.Id, gpu.NodeName, gpu.NodeIp, strconv.Itoa(int(gpu.Index)),
			gpu.Type, gpu.DriverVersion, strconv.Itoa(gpu.FrameworkVersion)}...)
	ch <- prometheus.MustNewConstMetric(xpuGpuPowerUsageDesc, prometheus.GaugeValue,
		float64(gpu.PowerUsage), []string{gpu.Id, gpu.NodeName, gpu.NodeIp, strconv.Itoa(int(gpu.Index)),
			gpu.Type, gpu.DriverVersion, strconv.Itoa(gpu.FrameworkVersion)}...)
	ch <- prometheus.MustNewConstMetric(xpuGpuTemperatureDesc, prometheus.GaugeValue,
		float64(gpu.Temperature), []string{gpu.Id, gpu.NodeName, gpu.NodeIp, strconv.Itoa(int(gpu.Index)),
			gpu.Type, gpu.DriverVersion, strconv.Itoa(gpu.FrameworkVersion)}...)
	ch <- prometheus.MustNewConstMetric(xpuVgpuNumberDesc, prometheus.GaugeValue, float64(len(gpu.VxpuDeviceList)),
		[]string{gpu.NodeName, gpu.NodeIp, gpu.Id}...)
}

func updateVgpuDeviceInfo(ch chan<- prometheus.Metric, gpu *utils.XPUDevice) {
	if !validate(ch) {
		log.Warningln("Invalid param in function updateVgpuDeviceInfo")
		return
	}
	var vgpuPodNumber = 0
	vgpuPodMap := make(map[string]int)
	for _, vgpu := range gpu.VxpuDeviceList {
		ch <- prometheus.MustNewConstMetric(xpuVgpuUtilizationDesc, prometheus.GaugeValue,
			vgpu.VxpuCoreUtilization, []string{gpu.Id, gpu.NodeName, gpu.NodeIp, vgpu.PodUID,
				vgpu.ContainerName, vgpu.Id, strconv.Itoa(int(vgpu.VxpuCoreLimit)),
				strconv.Itoa(int(vgpu.VxpuMemoryLimit))})
		ch <- prometheus.MustNewConstMetric(xpuVgpuMemoryUtilizationDesc, prometheus.GaugeValue,
			vgpu.VxpuMemoryUtilization, []string{gpu.Id, gpu.NodeName, gpu.NodeIp, vgpu.PodUID,
				vgpu.ContainerName, vgpu.Id, strconv.Itoa(int(vgpu.VxpuCoreLimit)),
				strconv.Itoa(int(vgpu.VxpuMemoryLimit))})
		if _, ok := vgpuPodMap[vgpu.PodUID]; !ok {
			vgpuPodNumber += 1
			vgpuPodMap[vgpu.PodUID] = vgpuPodNumber
		}
	}
	ch <- prometheus.MustNewConstMetric(xpuVgpuPodNumberDesc, prometheus.GaugeValue, float64(vgpuPodNumber),
		[]string{gpu.NodeName, gpu.NodeIp, gpu.Id}...)
}

func validate(ch chan<- prometheus.Metric, objs ...interface{}) bool {
	if ch == nil {
		return false
	}
	for _, v := range objs {
		val := reflect.ValueOf(v)
		if val.Kind() != reflect.Ptr {
			return false
		}
		if val.IsNil() {
			return false
		}
	}
	return true
}
