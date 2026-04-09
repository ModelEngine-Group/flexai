package gonvml

import "C"

type MemoryV2 struct {
	Version  uint32
	Total    uint64
	Reserved uint64
	Free     uint64
	Used     uint64
}

type Utilization struct {
	Gpu    uint32
	Memory uint32
}

type ProcessInfoV1 struct {
	Pid           uint32
	UsedGpuMemory uint64
}

type ProcessUtilizationSample struct {
	Pid       uint32
	TimeStamp uint64
	SmUtil    uint32
	MemUtil   uint32
	EncUtil   uint32
	DecUtil   uint32
}
