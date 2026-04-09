/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

// Package gonvml implements accessing the NVML library using the go

package gonvml

// EventData nvml EventData struct
type EventData struct {
	Device            Device
	EventType         uint64
	EventData         uint64
	GpuInstanceId     uint32
	ComputeInstanceId uint32
}

func (e EventData) convert() nvmlEventData {
	out := nvmlEventData{
		Device:            e.Device.(nvmlDevice),
		EventType:         e.EventType,
		EventData:         e.EventData,
		GpuInstanceId:     e.GpuInstanceId,
		ComputeInstanceId: e.ComputeInstanceId,
	}
	return out
}

type nvmlEventData struct {
	Device            nvmlDevice
	EventType         uint64
	EventData         uint64
	GpuInstanceId     uint32
	ComputeInstanceId uint32
}

func (e nvmlEventData) convert() EventData {
	out := EventData{
		Device:            e.Device,
		EventType:         e.EventType,
		EventData:         e.EventData,
		GpuInstanceId:     e.GpuInstanceId,
		ComputeInstanceId: e.ComputeInstanceId,
	}
	return out
}

// Wait wrapper, call nvmlEventSetWait
func (set nvmlEventSet) Wait(timeouts uint32) (EventData, NvmlRetType) {
	var data nvmlEventData
	ret := nvmlEventSetWaitWrapper(set, &data, timeouts)
	return data.convert(), ret
}

// Free wrapper, call nvmlEventSetFree
func (set nvmlEventSet) Free() NvmlRetType {
	return nvmlEventSetFreeWrapper(set)
}
