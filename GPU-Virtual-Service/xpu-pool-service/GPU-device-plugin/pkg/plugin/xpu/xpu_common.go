/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

// Package xpu defines and implements device abstraction layer
package xpu

import (
	"k8s.io/kubelet/pkg/apis/deviceplugin/v1beta1"
)

// Device couples an underlying v1beta1.Device type with its device node paths
type Device struct {
	v1beta1.Device
	LogicID  int32
	PhysicID int32
}

// IDeviceManager provides an interface for listing a set of Devices and checking health on them
type IDeviceManager interface {
	Devices() []*Device
	CheckHealth(stop <-chan interface{}, devices []*Device, unhealthy chan<- *Device)
}
