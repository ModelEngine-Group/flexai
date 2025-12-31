/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

// Package plugin implements vxpu device plugin
package plugin

import (
	"os"
	"strconv"
	"strings"
	"time"

	"gopkg.in/yaml.v2"

	"huawei.com/vxpu-device-plugin/pkg/graph"
	"huawei.com/vxpu-device-plugin/pkg/log"
	"huawei.com/vxpu-device-plugin/pkg/plugin/config"
	"huawei.com/vxpu-device-plugin/pkg/plugin/types"
	"huawei.com/vxpu-device-plugin/pkg/plugin/util"
	"huawei.com/vxpu-device-plugin/pkg/plugin/xpu"
)

const (
	failRetryInterval = 5
	registerInterval  = 30
)

// DeviceRegister register and patch vxpu information to the node annotation
type DeviceRegister struct {
	deviceCache      *DeviceCache
	topologyProvider graph.TopologyProvider
}

// NewDeviceRegister new a device register instance
func NewDeviceRegister(deviceCache *DeviceCache) *DeviceRegister {
	return &DeviceRegister{
		deviceCache:      deviceCache,
		topologyProvider: xpu.NewTopologyProvider(),
	}
}

// Start register and patch periodically
func (r *DeviceRegister) Start() {
	go r.watchAndRegister()
}

func (r *DeviceRegister) apiDevices() []*types.DeviceInfo {
	devs := r.deviceCache.GetCache()
	return xpu.GetDeviceInfo(devs)
}

func (r *DeviceRegister) registerInAnnotation() error {
	devices := r.apiDevices()
	encodedDevices := util.EncodeNodeDevices(devices)
	annotations := map[string]string{
		xpu.NodeVXPURegister:  encodedDevices,
		xpu.NodeVXPUHandshake: "Reported_" + time.Now().Format("2006.01.02 15:04:05"),
		xpu.NodeXpuTopology:   r.topologyProvider.Topology(),
	}

	log.Infoln("Reporting devices", encodedDevices, "in", time.Now().Format("2006.01.02 15:04:05"))
	err := util.PatchNodeAnnotations(config.NodeName, annotations)
	if err != nil {
		log.Errorln("k8s patch node error:", err.Error(), "node name:", config.NodeName)
		return err
	}
	return nil
}

func (r *DeviceRegister) initPatchNodeVxpuUsed() {
	node, err := util.GetNode(config.NodeName)
	if err != nil {
		log.Errorf("k8s get node error: %v, node name: %s", err, config.NodeName)
		return
	}
	if _, ok := node.ObjectMeta.Annotations[xpu.NodeVXPUUsed]; ok {
		log.Infof("node annotation %s already exists", xpu.NodeVXPUUsed)
		return
	} else {
		log.Infof("node annotation %s not exists, initialize it", xpu.NodeVXPUUsed)
	}

	var usedXPUs strings.Builder
	devs := r.deviceCache.GetCache()
	for _, dev := range devs {
		usedXPUs.Write([]byte(strconv.Itoa(int(dev.LogicID))))
		usedXPUs.Write([]byte(","))
		usedXPUs.Write([]byte(dev.ID))
		usedXPUs.Write([]byte(",0,0,0:"))
	}
	annos := make(map[string]string)
	annos[xpu.NodeVXPUUsed] = usedXPUs.String()
	err = util.PatchNodeAnnotations(config.NodeName, annos)
	if err != nil {
		log.Errorf("k8s patch node error: %v, node name: %s", err, config.NodeName)
	}
}

func (r *DeviceRegister) watchAndRegister() {
	log.Infof("into watchAndRegister")
	r.initPatchNodeVxpuUsed()
	if len(config.GPUTypeConfig) != 0 {
		loadGPUTypeConf()
	}
	lastSucceed := true
	for {
		err := r.registerInAnnotation()
		if err != nil {
			if lastSucceed == false {
				break
			}
			lastSucceed = false
			log.Errorln("register vxpu failed once, try again.")
			time.Sleep(time.Second * failRetryInterval)
		} else {
			lastSucceed = true
			time.Sleep(time.Second * registerInterval)
		}
	}
	log.Fatalln("register vxpu failed twice, exit program!")
}

func loadGPUTypeConf() {
	confData, err := os.ReadFile(config.GPUTypeConfig)
	if err != nil {
		log.Errorf("Failed to read gpu type config in '%s', err: %v", config.GPUTypeConfig, err)
		return
	}
	conf := strings.TrimSpace(string(confData))
	unmarshalGPUTypeConf(conf)
}

func unmarshalGPUTypeConf(confStr string) {
	config.GPUTypeMap = make(map[string]string)
	if err := yaml.Unmarshal([]byte(confStr), &config.GPUTypeMap); err != nil {
		log.Errorf("Failed to unmarshal gpu type yaml, err: %v", err)
		return
	}
	log.Infof("unmarshal gpu type succeed, content: %v", config.GPUTypeMap)
}
