/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

// Package gonvml implements accessing the NVML library using the go
package gonvml

import (
	"C"
	"fmt"
	"reflect"
)

const deviceGetMemInfoVersion = 2

var pidMaxSize uint32 = 1024

func (device nvmlDevice) GetMemoryInfoV2() (MemoryV2, NvmlRetType) {
	var memory MemoryV2
	memory.Version = structVersion(memory, deviceGetMemInfoVersion)
	ret := nvmlDeviceGetMemoryInfoWrapper(device, &memory)
	return memory, ret
}

func (device nvmlDevice) GetName() (string, NvmlRetType) {
	name := make([]byte, DeviceNameV2BufferSize)
	ret := nvmlDeviceGetNameWrapper(device, &name[0], DeviceNameV2BufferSize)
	return string(name[:clen(name)]), ret
}

func (device nvmlDevice) RegisterEvents(eventTypes uint64, set EventSet) NvmlRetType {
	return nvmlDeviceRegisterEventsWrapper(device, eventTypes, set.(nvmlEventSet))
}

func (device nvmlDevice) GetUUID() (string, NvmlRetType) {
	uuid := make([]byte, DeviceUUIDV2BufferSize)
	ret := nvmlDeviceGetUUIDWrapper(device, &uuid[0], DeviceUUIDV2BufferSize)
	return string(uuid[:clen(uuid)]), ret
}

func (device nvmlDevice) GetIndex() (int, NvmlRetType) {
	var index uint32
	ret := nvmlDeviceGetIndexWrapper(device, &index)
	return int(index), ret
}

func (device nvmlDevice) GetUtilizationRates() (Utilization, NvmlRetType) {
	var utilization Utilization
	ret := nvmlDeviceGetUtilizationRatesWrapper(device, &utilization)
	return utilization, ret
}

func (device nvmlDevice) GetComputeRunningProcesses() ([]ProcessInfoV1, NvmlRetType) {
	var infoSize = pidMaxSize
	infos := make([]ProcessInfoV1, infoSize)
	ret := nvmlDeviceGetComputeRunningProcessesWrapper(device, &infoSize, &infos[0])
	return infos, ret
}

func (device nvmlDevice) DeviceGetProcessUtilization(timestamp uint64) ([]ProcessUtilizationSample, NvmlRetType) {
	var sampleSize = pidMaxSize
	samples := make([]ProcessUtilizationSample, sampleSize)
	ret := nvmlDeviceGetProcessUtilizationWrapper(device, &samples[0], &sampleSize, timestamp)
	return samples, ret
}

func (device nvmlDevice) GetMultiGpuBoard() (int, NvmlRetType) {
	var multiGpuBoard uint32
	ret := nvmlDeviceGetMultiGpuBoardWrapper(device, &multiGpuBoard)
	return int(multiGpuBoard), ret
}

func (device1 nvmlDevice) GetTopologyCommonAncestor(device2 Device) (GpuTopologyLevel, NvmlRetType) {
	var pathInfo GpuTopologyLevel
	ret := nvmlDeviceGetTopologyCommonAncestorWrapper(device1, nvmlDeviceHandle(device2), &pathInfo)
	return pathInfo, ret
}

func nvmlDeviceHandle(d Device) nvmlDevice {
	var helper func(val reflect.Value) nvmlDevice
	helper = func(val reflect.Value) nvmlDevice {
		if val.Kind() == reflect.Interface {
			val = val.Elem()
		}

		if val.Kind() == reflect.Ptr {
			val = val.Elem()
		}

		if val.Type() == reflect.TypeOf(nvmlDevice{}) {
			return val.Interface().(nvmlDevice)
		}

		if val.Kind() != reflect.Struct {
			panic("unexpected type")
		}

		for i := 0; i < val.Type().NumField(); i++ {
			if !val.Type().Field(i).Anonymous {
				continue
			}
			if !val.Field(i).Type().Implements(reflect.TypeOf((*Device)(nil)).Elem()) {
				continue
			}
			return helper(val.Field(i))
		}
		panic(fmt.Errorf("unable to convert %T to NvmlDevice", d))
	}
	return helper(reflect.ValueOf(d))
}

func (device nvmlDevice) GetTopologyNearestGpus(level GpuTopologyLevel) ([]Device, NvmlRetType) {
	var count uint32
	ret := nvmlDeviceGetTopologyNearestGpusWrapper(device, level, &count, nil)
	if ret != Success {
		return nil, ret
	}
	if count == 0 {
		return []Device{}, ret
	}
	deviceArray := make([]nvmlDevice, count)
	ret = nvmlDeviceGetTopologyNearestGpusWrapper(device, level, &count, &deviceArray[0])
	return convertSlice[nvmlDevice, Device](deviceArray), ret
}

func (device nvmlDevice) GetTemperature(temperatureGpu NvmlTemperatureSensors) (uint32, NvmlRetType) {
	var temp uint32
	ret := nvmlDeviceGetTemperatureWrapper(device, temperatureGpu, &temp)
	return temp, ret
}

func (device nvmlDevice) GetPowerUsage() (uint32, NvmlRetType) {
	var power uint32
	ret := nvmlDeviceGetPowerUsageWrapper(device, &power)
	return power, ret
}
