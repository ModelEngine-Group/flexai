/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

// Package plugin implements vxpu device plugin
package plugin

import (
	"sync"

	"k8s.io/kubelet/pkg/apis/deviceplugin/v1beta1"

	"huawei.com/vxpu-device-plugin/pkg/plugin/xpu"
)

// DeviceCache provide xpu device cache for plugin and register
type DeviceCache struct {
	xpu.DeviceManager
	cache     []*xpu.Device
	stopCh    chan interface{}
	unhealthy chan *xpu.Device
	notifyCh  map[string]chan *xpu.Device
	mutex     sync.Mutex
}

// NewDeviceCache new a DeviceCache instance
func NewDeviceCache() *DeviceCache {
	return &DeviceCache{
		stopCh:    make(chan interface{}),
		unhealthy: make(chan *xpu.Device),
		notifyCh:  make(map[string]chan *xpu.Device),
	}
}

// AddNotifyChannel add notify channel for unhealthy or stop event
func (d *DeviceCache) AddNotifyChannel(name string, ch chan *xpu.Device) {
	d.mutex.Lock()
	defer d.mutex.Unlock()
	d.notifyCh[name] = ch
}

// RemoveNotifyChannel remove notify channel
func (d *DeviceCache) RemoveNotifyChannel(name string) {
	d.mutex.Lock()
	defer d.mutex.Unlock()
	delete(d.notifyCh, name)
}

// Start health check and notify loop
func (d *DeviceCache) Start() {
	d.cache = d.Devices()
	go d.CheckHealth(d.stopCh, d.cache, d.unhealthy)
	go d.notifyLoop()
}

// Stop health check and notify loop
func (d *DeviceCache) Stop() {
	close(d.stopCh)
}

// GetCache get xpu devices cache
func (d *DeviceCache) GetCache() []*xpu.Device {
	return d.cache
}

func (d *DeviceCache) notifyLoop() {
	for {
		select {
		case <-d.stopCh:
			return
		case dev := <-d.unhealthy:
			dev.Health = v1beta1.Unhealthy
			d.notify(dev)
		}
	}
}

func (d *DeviceCache) notify(dev *xpu.Device) {
	d.mutex.Lock()
	for _, ch := range d.notifyCh {
		if ch != nil {
			ch <- dev
		}
	}
	d.mutex.Unlock()
}
