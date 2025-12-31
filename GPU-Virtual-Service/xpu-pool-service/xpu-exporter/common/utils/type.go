/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

// Package utils defines data structure for xpu-exporter
package utils

// XPUDevice description of GPU
type XPUDevice struct {
	Index             int32
	Id                string
	Type              string
	Health            bool
	Count             uint32
	MemoryTotal       uint64
	MemoryUsed        uint64
	MemoryUtilization float64
	XpuUtilization    float64
	NodeName          string
	NodeIp            string
	DriverVersion     string
	FrameworkVersion  int
	PowerUsage        int
	Temperature       int
	VxpuDeviceList    VxpuDevices
}

// VxpuDevices description of all vxpus in the pod
type VxpuDevices []VxpuDevice

// VxpuDevice description of store vxpu related information
type VxpuDevice struct {
	Id                    string
	GpuId                 string
	PodUID                string
	ContainerName         string
	VxpuMemoryUsed        uint64
	VxpuMemoryUtilization float64
	VxpuCoreUtilization   float64
	VxpuMemoryLimit       int64
	VxpuCoreLimit         int64
}
