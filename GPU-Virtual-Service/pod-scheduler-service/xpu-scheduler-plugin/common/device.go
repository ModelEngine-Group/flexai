/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

package common

type XPUDevice struct {
	Index      int
	Id         string
	NodeId     string
	Type       string
	Health     bool
	Cores      int
	Memory     uint64
	Count      int
	UsedCores  int
	UsedMemory uint64
	UsedVids   uint
	InUse      bool
	Numa       int
}

func (x *XPUDevice) GetVidBound() uint {
	count := uint(0)
	vids := x.UsedVids
	for vids != 0 {
		count += vids & 1
		vids >>= 1
	}
	return count
}

func (x *XPUDevice) AllocVid() uint {
	vid := uint(0)
	vidBit := (x.UsedVids + 1) ^ x.UsedVids
	x.UsedVids |= vidBit
	for vidBit > 1 {
		vid++
		vidBit >>= 1
	}
	return vid
}

func (x *XPUDevice) OccupyVid(vid uint) bool {
	vidBit := uint(1) << vid
	ok := x.UsedVids&vidBit != 0
	x.UsedVids |= vidBit
	return ok
}

type ContainerDevice struct {
	Index      int
	Id         string
	Type       string
	UsedMemory uint64
	UsedCores  int
	Vid        uint
}
