/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

// Package types defines constants and data structure
package types

const (
	// DeviceBindTime allocate bind time
	DeviceBindTime = "huawei.com/bind-time"
	// DeviceBindPhase allocate bind phase
	DeviceBindPhase = "huawei.com/bind-phase"

	// DeviceBindAllocating bind phase allocating
	DeviceBindAllocating = "allocating"
	// DeviceBindFailed bind phase failed
	DeviceBindFailed = "failed"
	// DeviceBindSuccess bind phase success
	DeviceBindSuccess = "success"

	// VXPULockName lockname used to lock a node
	VXPULockName = "vxpu"
)

// ContainerDevice description of one vxpu in the container
type ContainerDevice struct {
	Index     int32
	UUID      string
	Type      string
	Usedmem   int32
	Usedcores int32
	Vid       int32
}

// ContainerDevices description of all vxpus in the container
type ContainerDevices []ContainerDevice

// PodDevices description of all vxpus in the pod
type PodDevices []ContainerDevices

// DeviceInfo description of xpu registered in the node annotation
type DeviceInfo struct {
	Index  int32  `protobuf:"varint,1,opt,name=index,proto3" json:"index,omitempty"`
	Id     string `protobuf:"bytes,2,opt,name=id,proto3" json:"id,omitempty"`
	Count  int32  `protobuf:"varint,3,opt,name=count,proto3" json:"count,omitempty"`
	Devmem int32  `protobuf:"varint,4,opt,name=devmem,proto3" json:"devmem,omitempty"`
	Type   string `protobuf:"bytes,5,opt,name=type,proto3" json:"type,omitempty"`
	Health bool   `protobuf:"varint,6,opt,name=health,proto3" json:"health,omitempty"`
	Numa   int32  `protobuf:"varint,7,opt,name=numa,proto3" json:"numa,omitempty"`
}

// XPUDevice description of xpu
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
	PowerUsage        uint32
	Temperature       uint32
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

// ProcessUsage description of process usage on xpu
type ProcessUsage struct {
	ProcessMem             uint64
	ProcessCoreUtilization uint64
}

// DeviceUsageInfo description of device usage
type DeviceUsageInfo struct {
	CoreUtil    uint32
	MemUtil     uint32
	PowerUsage  uint32
	Temperature uint32
}
