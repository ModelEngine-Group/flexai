/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

// This file defines the API provided by the gonvml.
// Package gonvml implements accessing the NVML library using the go

package gonvml

// NvmlInterface interface
type nvmlInterface interface {
	Init() NvmlRetType
	InitWithFlags(uint32) NvmlRetType
	Shutdown() NvmlRetType
}

// Device define nvml device interface
type Device interface {
	GetMemoryInfoV2() (MemoryV2, NvmlRetType)
	GetName() (string, NvmlRetType)
	RegisterEvents(uint64, EventSet) NvmlRetType
	GetUUID() (string, NvmlRetType)
	GetIndex() (int, NvmlRetType)
	GetUtilizationRates() (Utilization, NvmlRetType)
	GetComputeRunningProcesses() ([]ProcessInfoV1, NvmlRetType)
	DeviceGetProcessUtilization(timestamp uint64) ([]ProcessUtilizationSample, NvmlRetType)
	GetMultiGpuBoard() (int, NvmlRetType)
	GetTopologyCommonAncestor(Device) (GpuTopologyLevel, NvmlRetType)
	GetTopologyNearestGpus(GpuTopologyLevel) ([]Device, NvmlRetType)
	GetTemperature(NvmlTemperatureSensors) (uint32, NvmlRetType)
	GetPowerUsage() (uint32, NvmlRetType)
}

// EventSet define nvml EventSet interface
type EventSet interface {
	Free() NvmlRetType
	Wait(uint32) (EventData, NvmlRetType)
}

var (
	// Init nvmlInit api adapter
	Init = libnvml.Init

	// InitWithFlags nvmlInitWithFlags api adapter
	InitWithFlags = libnvml.InitWithFlags

	// Shutdown nvmlShutdown api adapter
	Shutdown = libnvml.Shutdown

	// DeviceGetCount nvmlDeviceGetCount api adapter
	DeviceGetCount = libnvml.DeviceGetCount

	// SystemGetDriverVersion nvmlSystemGetDriverVersion api adapter
	SystemGetDriverVersion = libnvml.SystemGetDriverVersion

	// SystemGetCudaDriverVersion nvmlSystemGetCudaDriverVersion api adapter
	SystemGetCudaDriverVersion = libnvml.SystemGetCudaDriverVersion

	// DeviceGetHandleByIndex nvmlDeviceGetHandleByIndex api adapter
	DeviceGetHandleByIndex = libnvml.DeviceGetHandleByIndex

	// DeviceGetHandleByUUID nvmlDeviceGetHandleByUUID api adapter
	DeviceGetHandleByUUID = libnvml.DeviceGetHandleByUUID

	// DeviceRegisterEvents nvmlDeviceRegisterEvents api adapter
	DeviceRegisterEvents = libnvml.DeviceRegisterEvents

	// EventSetCreate nvmlEventSetCreate api adapter
	EventSetCreate = libnvml.EventSetCreate

	// EventSetFree nvmlEventSetFree api adapter
	EventSetFree = libnvml.EventSetFree

	// EventSetWait nvmlEventSetWait api adapter
	EventSetWait = libnvml.EventSetWait

	// DeviceGetTopologyCommonAncestor nvmlDeviceGetTopologyCommonAncestor
	DeviceGetTopologyCommonAncestor = libnvml.DeviceGetTopologyCommonAncestor

	// DeviceGetTopologyNearestGpus nvmlDeviceGetTopologyNearestGpus
	DeviceGetTopologyNearestGpus = libnvml.DeviceGetTopologyNearestGpus

	// DeviceGetMultiGpuBoard nvmlDeviceGetMultiGpuBoard
	DeviceGetMultiGpuBoard = libnvml.DeviceGetMultiGpuBoard
)
