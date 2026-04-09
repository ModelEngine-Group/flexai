/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

// Package gonvml implements accessing the NVML library using the go

package gonvml

// Refcount Defines the reference counting class Refcount
type Refcount int

// IncNoError Increase a reference counting
func (r *Refcount) IncNoError(err error) {
	if err == nil {
		(*r)++
	}
}

// DecNoError Decrease Increase reference counting
func (r *Refcount) DecNoError(err error) {
	if err == nil && (*r) > 0 {
		(*r)--
	}
}
