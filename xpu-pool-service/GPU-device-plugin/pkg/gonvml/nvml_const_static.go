/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

// This file implements basic static gonvml functions.
// Package gonvml implements accessing the NVML library using the go

package gonvml

import (
	"reflect"
)

const (
	versionShift = 24 // The shift count to move the version into the high bytes of the result.
)

func structVersion(data interface{}, version uint32) uint32 {
	return uint32(reflect.Indirect(reflect.ValueOf(data)).Type().Size()) | (version << uint32(versionShift))
}

func clen(n []byte) int {
	for i := 0; i < len(n); i++ {
		if n[i] == 0 {
			return i
		}
	}
	return len(n)
}
