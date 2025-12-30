/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

// The gonvml constant definition is implemented in this file.
// Package gonvml implements accessing the NVML library using the go

package gonvml

// NvmlRetType the type of nvml api return value
type NvmlRetType int32

// MemoryErrorType the type of memory error
type MemoryErrorType int32

// The letter case of the constant name is the same as that in nvml.h.
const (
	// EventypexidCriticalError as defined in nvml/nvml.h
	EventTypeXidCriticalError = 8

	// DeviceNameV2BufferSize as defined in nvml/nvml.h
	DeviceNameV2BufferSize = 96

	// DeviceUUIDBufferSize as defined in nvml/nvml.h
	DeviceUUIDV2BufferSize = 96

	// SystemDriverVersionBufferSize as defined in nvml/nvml.h
	SystemDriverVersionBufferSize = 88
)

// Return enumeration from nvml/nvml.h
const (
	Success NvmlRetType = iota
	ErrorUninitialized
	ErrorInvalidArgument
	ErrorNotSupported
	ErrorNoPermission
	ErrorAlreadyInitialized
	ErrorNotFound NvmlRetType = 6
	ErrorInsufficientSize
	ErrorInsufficientPower
	ErrorDriverNotLoaded
	ErrorTimeout
	ErrorIrqIssue
	ErrorLibraryNotFound NvmlRetType = 12
	ErrorFunctionNotFound
	ErrorCorruptedInfo
	ErrorGpuIsLost
	ErrorResetRequired
	ErrorOperatingSystem
	ErrorLibRMVersionMismatch
	ErrorInsue
	ErrorInvalidState
	ErrorArchMismatch
	ErrorGpuNotSupported
	ErrorInsufficientResources
	ErrorFirmwareNotSupported
	ErrorDeprecated
	ErrorUnknown NvmlRetType = 999
)

// GpuTopologyLevel as declared in nvml/nvml.h
type GpuTopologyLevel int32

// GpuTopologyLevel enumeration from nvml/nvml.h
const (
	TopologyInternal   GpuTopologyLevel = iota
	TopologySingle     GpuTopologyLevel = 10
	TopologyMultiple   GpuTopologyLevel = 20
	TopologyHostbridge GpuTopologyLevel = 30
	TopologyNode       GpuTopologyLevel = 40
	TopologySystem     GpuTopologyLevel = 50
)

type NvmlTemperatureSensors int32

const (
	NvmlTemperatureGpu NvmlTemperatureSensors = 0
)
