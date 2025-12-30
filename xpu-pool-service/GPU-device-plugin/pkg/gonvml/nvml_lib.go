/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

// Package gonvml implements accessing the NVML library using the go

package gonvml

import "C"
import (
	"fmt"
	"sync"
)

type library struct {
	sync.Mutex
	refcount Refcount
}

var libnvml = newLibrary()

func newLibrary() *library {
	l := &library{}
	return l
}

func (l *library) load() (err error) {
	l.Lock()
	defer l.Unlock()

	defer func() { l.refcount.IncNoError(err) }()
	if l.refcount > 0 {
		return nil
	}

	if err := loadNvmlSo(); err != nil {
		return fmt.Errorf("error opening nvml-el.so: %w", err)
	}
	return nil
}

func (l *library) close() (err error) {
	l.Lock()
	defer l.Unlock()

	defer func() { l.refcount.DecNoError(err) }()
	if l.refcount != 1 {
		return nil
	}

	if err := unloadNvmlSo(); err != nil {
		return fmt.Errorf("error closing nvml-el.so: %w", err)
	}
	return nil
}

func (l *library) Init() NvmlRetType {
	if err := l.load(); err != nil {
		return ErrorLibraryNotFound
	}
	return nvmlInitWrapper()
}

func (l *library) InitWithFlags(flags uint32) NvmlRetType {
	if err := l.load(); err != nil {
		return ErrorLibraryNotFound
	}
	return nvmlInitWithFlagsWrapper(flags)
}

func (l *library) Shutdown() NvmlRetType {
	ret := nvmlShutdownWrapper()
	if ret != Success {
		return ret
	}
	err := l.close()
	if err != nil {
		return ErrorUnknown
	}
	return ret
}

func (l *library) DeviceGetCount() (int, NvmlRetType) {
	var DeviceCount uint32
	ret := nvmlDeviceGetCountWrapper(&DeviceCount)
	return int(DeviceCount), ret
}

func (l *library) SystemGetDriverVersion() (string, NvmlRetType) {
	Version := make([]byte, SystemDriverVersionBufferSize)
	ret := nvmlSystemGetDriverVersionWrapper(&Version[0], SystemDriverVersionBufferSize)
	return string(Version[:clen(Version)]), ret
}

func (l *library) SystemGetCudaDriverVersion() (int, NvmlRetType) {
	var CudaDriverVersion int32
	ret := nvmlSystemGetCudaDriverVersionWrapper(&CudaDriverVersion)
	return int(CudaDriverVersion), ret
}

func (l *library) DeviceGetHandleByIndex(index int) (Device, NvmlRetType) {
	var device nvmlDevice
	ret := nvmlDeviceGetHandleByIndexWrapper(uint32(index), &device)
	return device, ret
}

func (l *library) DeviceGetHandleByUUID(uuid string) (Device, NvmlRetType) {
	var device nvmlDevice
	ret := nvmlDeviceGetHandleByUUIDWrapper(uuid+string(rune(0)), &device)
	return device, ret
}

func (l *library) DeviceRegisterEvents(device Device, eventTypes uint64, set EventSet) NvmlRetType {
	return device.RegisterEvents(eventTypes, set)
}

func (l *library) EventSetCreate() (EventSet, NvmlRetType) {
	var set nvmlEventSet
	ret := nvmlEventSetCreateWrapper(&set)
	return set, ret
}

func (l *library) EventSetWait(set EventSet, timeouts uint32) (EventData, NvmlRetType) {
	return set.Wait(timeouts)
}

func (l *library) EventSetFree(set EventSet) NvmlRetType {
	return set.Free()
}

func (l *library) DeviceGetMultiGpuBoard(device Device) (int, NvmlRetType) {
	return device.GetMultiGpuBoard()
}

func (l *library) DeviceGetTopologyCommonAncestor(device1 Device, device2 Device) (GpuTopologyLevel, NvmlRetType) {
	return device1.GetTopologyCommonAncestor(device2)
}

func (l *library) DeviceGetTopologyNearestGpus(device Device, level GpuTopologyLevel) ([]Device, NvmlRetType) {
	return device.GetTopologyNearestGpus(level)
}
