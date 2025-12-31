/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

// Package util defines data structure and provide util function for xpu scheduler plugin implementation
package util

const (
	// LogErrorLevel for log error.
	LogErrorLevel = 1
	// LogWarningLevel for log warning.
	LogWarningLevel = 2
	// LogInfoLevel for log information.
	LogInfoLevel = 3
	// LogDebugLevel for log debug.
	LogDebugLevel = 4
	// ArgumentError argument nil error.
	ArgumentError = "invalid argument"
	// ObjectNilError object or argument nil error.
	ObjectNilError = "object or argument is nil"
	// Comma for ,
	Comma = ","
	// Semicolon for ;
	Semicolon = ";"
	// XPUHexKilo for const 1000, volcano frame used.
	XPUHexKilo = 1000
	// MapInitNum for map init length.
	MapInitNum = 3
	// XPUDeviceLen for xpu device param number.
	XPUDeviceLen = 7
	// Base2 for const 2.
	Base2 = 2
	// Base10 for const 10.
	Base10 = 10
	// Base100 for const 100.
	Base100 = 100
	// Base1024 for const 1024.
	Base1024 = 1024
	// TopologyMinNum for topology gpu device min num
	TopologyMinNum = 1
	// TopologyMaxNum for topology gpu device max num
	TopologyMaxNum = 32
	// CoreWeight for binpack memory weight
	CoreWeight = 1
	// MemoryWeight for binpack memory weight
	MemoryWeight = 1
	// HandshakeTolerateUpdateTime for handshake tolerate upload time
	HandshakeTolerateUpdateTime = 60
	// XpuMultiplier for calculate score without topology
	XpuMultiplier = 100
)

const (
	// PodGroupInqueue the pg Inqueue status
	PodGroupInqueue = "Inqueue"
	// PodGroupPending the pg Pending status
	PodGroupPending = "Pending"
	// PodGroupRunning the pg Running status
	PodGroupRunning = "Running"
	// PodGroupUnknown the pg Unknown status
	PodGroupUnknown = "Unknown"
	// PodGroupUnschedulableType the pg Unschedulable Condition
	PodGroupUnschedulableType = "Unschedulable"
	// PodDeleteTimes the tag of single pod has been deleted
	PodDeleteTimes = "pod-delete-times"
	// TagOfPodPending the limitation on pod pending times
	TagOfPodPending = "ready"
	// JobRestartReason the reason of job restart
	JobRestartReason = "task reach max pending session"
)

const (
	// GPUPluginName for GPU
	GPUPluginName = "gpu"
	// NPUPluginName for NPU
	NPUPluginName = "npu"

	// VGPUName for GPU card
	VGPUName = "huawei.com/vgpu-number"
	// VGPUType for GPU card, for example: huawei.com/vgpu-type.L20: 1
	VGPUType = "huawei.com/vgpu-type."
	// VGPUCore for vgpu core
	VGPUCore = "huawei.com/vgpu-cores"
	// VGPUMemory for vgpu memory
	VGPUMemory = "huawei.com/vgpu-memory.1Gi"
	// NvidiaGPUDevice device type supported by the device plugin
	NvidiaGPUDevice = "GPU"
	// NodeGPURegisterAnnotation for gpu register annotation
	NodeGPURegisterAnnotation = "huawei.com/node-vgpu-register"
	// AssignedGPUsToAllocateAnnotations pod's gpu devices to allocate to container
	AssignedGPUsToAllocateAnnotations = "huawei.com/vgpu-devices-to-allocate"
	// AssignedGPUsToPodAnnotations pod's devices to allocate to container by scheduler
	AssignedGPUsToPodAnnotations = "huawei.com/vgpu-ids-new"
	// AssignedGPUsToNodeAnnotations assigned node name
	AssignedGPUsToNodeAnnotations = "huawei.com/vgpu-node"
	// NodeGPUTopologyAnnotation for node gpu topology
	NodeGPUTopologyAnnotation = "huawei.com/node-gpu-topology"
	// NodeGPUHandshakeAnnotation handshake timestamp for vgpu register
	NodeGPUHandshakeAnnotation = "huawei.com/node-vgpu-handshake"

	// VNPUName for NPU card
	VNPUName = "huawei.com/vnpu-number"
	// VNPUType for NPU card, for example: huawei.com/vnpu-type.310P: 1
	VNPUType = "huawei.com/vnpu-type."
	// VNPUCore for vnpu core
	VNPUCore = "huawei.com/vnpu-cores"
	// VNPUMemory for vnpu memory
	VNPUMemory = "huawei.com/vnpu-memory.1Gi"
	// AscendNPUDevice device type supported by the device plugin
	AscendNPUDevice = "NPU"
	// NodeNPURegisterAnnotation for npu register annotation
	NodeNPURegisterAnnotation = "huawei.com/node-vnpu-register"
	// AssignedNPUsToAllocateAnnotations pod's npu devices to allocate to container
	AssignedNPUsToAllocateAnnotations = "huawei.com/vnpu-devices-to-allocate"
	// AssignedNPUsToPodAnnotations pod's devices to allocate to container by scheduler
	AssignedNPUsToPodAnnotations = "huawei.com/vnpu-ids-new"
	// AssignedNPUsToNodeAnnotations assigned node name
	AssignedNPUsToNodeAnnotations = "huawei.com/vnpu-node"
	// NodeNPUTopologyAnnotation for node npu topology
	NodeNPUTopologyAnnotation = "huawei.com/node-npu-topology"
	// NodeNPUHandshakeAnnotation handshake timestamp for vnpu register
	NodeNPUHandshakeAnnotation = "huawei.com/node-vnpu-handshake"

	// BindTimeAnnotations set pod BindTimeAnnotations for using by device-plugin.
	BindTimeAnnotations = "huawei.com/bind-time"
	// DeviceBindPhase allocate bind phase
	DeviceBindPhase = "huawei.com/bind-phase"
	// DeviceBindAllocating bind phase allocating
	DeviceBindAllocating = "allocating"
	// XPUTopologyIntraBandwidthAnnotation for minimum bandwidth rate between pod's xpus
	XPUTopologyIntraBandwidthAnnotation = "huawei.com/intra-bandwidth"
	// XPUTopologyTaskListAnnotation set order of task in XPUTopologyBandwidthAnnotation
	XPUTopologyTaskListAnnotation = "huawei.com/topology-task-list"
	// XPUTopologyInterBandwidthAnnotation for minimum bandwidth rate between pods,
	// should be an n*n matrix, n is task number in XPUTopologyTaskListAnnotation
	XPUTopologyInterBandwidthAnnotation = "huawei.com/inter-bandwidth"
	// TaskSpec set origin task name for pod
	TaskSpec = "volcano.sh/task-spec"
)

var (
	// XPUTopologyNodeBandwidth bandwidth between nodes, should be an n*n matrix, n is node number in XPUTopologyNodeList
	XPUTopologyNodeBandwidth map[string]map[string]int
)
