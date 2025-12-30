/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

// Package plugin implements vxpu device plugin
package plugin

import (
	"bufio"
	"context"
	"errors"
	"fmt"
	"net"
	"os"
	"path"
	"path/filepath"
	"time"

	"google.golang.org/grpc"
	"k8s.io/kubelet/pkg/apis/deviceplugin/v1beta1"

	"huawei.com/vxpu-device-plugin/pkg/lock"
	"huawei.com/vxpu-device-plugin/pkg/log"
	"huawei.com/vxpu-device-plugin/pkg/plugin/config"
	"huawei.com/vxpu-device-plugin/pkg/plugin/types"
	"huawei.com/vxpu-device-plugin/pkg/plugin/util"
	"huawei.com/vxpu-device-plugin/pkg/plugin/xpu"
)

const (
	grpcServeTryCount = 5
	secondsPerHour    = 3600
	dialTimeout       = 5
	pluginNotify      = "plugin"
	pluginSockPerm    = 0600
)

// DevicePlugin implements the Kubernetes device plugin API
type DevicePlugin struct {
	deviceCache  *DeviceCache
	resourceName string
	socket       string

	server *grpc.Server
	health chan *xpu.Device
	stop   chan interface{}
}

// NewDevicePlugin returns an initialized DevicePlugin
func NewDevicePlugin(resourceName string, deviceCache *DeviceCache, socket string) *DevicePlugin {
	return &DevicePlugin{
		deviceCache:  deviceCache,
		resourceName: resourceName,
		socket:       socket,

		// These will be reinitialized every time the plugin server is restarted.
		server: nil,
		health: nil,
		stop:   nil,
	}
}

func (m *DevicePlugin) initialize() {
	m.server = grpc.NewServer([]grpc.ServerOption{}...)
	m.health = make(chan *xpu.Device)
	m.stop = make(chan interface{})
}

func (m *DevicePlugin) cleanup() {
	close(m.stop)
	m.server = nil
	m.health = nil
	m.stop = nil
}

// Start starts the gRPC server, registers the device plugin with the Kubelet,
// and starts the device health checks
func (m *DevicePlugin) Start() error {
	m.initialize()

	err := m.serve()
	if err != nil {
		log.Errorf("Could not start device plugin for '%s': %s", m.resourceName, err)
		m.cleanup()
		return err
	}
	log.Infof("Starting to serve '%s' on %s", m.resourceName, m.socket)

	err = m.register()
	if err != nil {
		log.Errorf("Could not register device plugin: %s", err)
		m.Stop()
		return err
	}
	log.Infof("Registered device plugin for '%s' with Kubelet", m.resourceName)

	m.deviceCache.AddNotifyChannel(pluginNotify, m.health)
	return nil
}

// Stop stops the gRPC server.
func (m *DevicePlugin) Stop() {
	if m == nil || m.server == nil {
		return
	}
	log.Infof("Stopping to serve '%s' on %s", m.resourceName, m.socket)
	m.deviceCache.RemoveNotifyChannel(pluginNotify)
	m.server.Stop()
	if err := os.Remove(m.socket); err != nil && !os.IsNotExist(err) {
		log.Errorf("remove sock error: %v, path: %s", err, m.socket)
	}
	m.cleanup()
}

// serve starts the gRPC server of the device plugin.
func (m *DevicePlugin) serve() error {
	err := os.Remove(m.socket)
	if err != nil && !os.IsNotExist(err) {
		return err
	}
	sock, err := net.Listen("unix", m.socket)
	if err != nil {
		return err
	}
	err = os.Chmod(m.socket, pluginSockPerm)
	if err != nil {
		log.Errorf("modify plugin socket file permissions error: %v", err)
		return err
	}

	v1beta1.RegisterDevicePluginServer(m.server, m)

	go func() {
		lastCrashTime := time.Now()
		restartCount := 0
		for {
			log.Infof("Starting GRPC server for '%s'", m.resourceName)
			err := m.server.Serve(sock)
			if err == nil {
				break
			}

			log.Errorf("GRPC server for '%s' crashed with error: %v", m.resourceName, err)

			// restart if it has not been too often
			// i.e. if server has crashed more than 5 times and it didn't last more than one hour each time
			if restartCount > grpcServeTryCount {
				log.Fatalf("GRPC server for '%s' has repeatedly crashed recently. Quitting", m.resourceName)
			}
			timeSinceLastCrash := time.Since(lastCrashTime).Seconds()
			lastCrashTime = time.Now()
			if timeSinceLastCrash > secondsPerHour {
				restartCount = 1
			} else {
				restartCount++
			}
		}
	}()

	// Wait for server to start by launching a blocking connection
	conn, err := m.dial(m.socket, dialTimeout*time.Second)
	if err != nil {
		return err
	}
	if conn != nil {
		conn.Close()
	}

	return nil
}

// register registers the device plugin for the given resourceName with Kubelet.
func (m *DevicePlugin) register() error {
	conn, err := m.dial(v1beta1.KubeletSocket, dialTimeout*time.Second)
	if err != nil {
		return err
	}
	if conn == nil {
		return fmt.Errorf("client connection is nil")
	}
	defer conn.Close()

	client := v1beta1.NewRegistrationClient(conn)
	req := &v1beta1.RegisterRequest{
		Version:      v1beta1.Version,
		Endpoint:     path.Base(m.socket),
		ResourceName: m.resourceName,
		Options:      &v1beta1.DevicePluginOptions{},
	}

	_, err = client.Register(context.Background(), req)
	return err
}

// GetDevicePluginOptions returns the values of the optional settings for this plugin
func (m *DevicePlugin) GetDevicePluginOptions(context.Context, *v1beta1.Empty) (
	*v1beta1.DevicePluginOptions, error) {
	options := &v1beta1.DevicePluginOptions{}
	return options, nil
}

// GetPreferredAllocation returns a preferred set of devices to allocate
func (m *DevicePlugin) GetPreferredAllocation(context.Context, *v1beta1.PreferredAllocationRequest) (
	*v1beta1.PreferredAllocationResponse, error) {
	resp := &v1beta1.PreferredAllocationResponse{}
	return resp, nil
}

// ListAndWatch lists devices and update that list according to the health status
func (m *DevicePlugin) ListAndWatch(e *v1beta1.Empty, s v1beta1.DevicePlugin_ListAndWatchServer) error {
	_ = s.Send(&v1beta1.ListAndWatchResponse{Devices: m.apiDevices()})
	for {
		select {
		case <-m.stop:
			return nil
		case d := <-m.health:
			// Caution: there is no way to recover from the Unhealthy state.
			// d.Health -> v1beta1.Unhealthy, no need do it, since notifyLoop() in cache.go has done it
			log.Warningf("'%s' device marked unhealthy: %s", m.resourceName, d.ID)
			_ = s.Send(&v1beta1.ListAndWatchResponse{Devices: m.apiDevices()})
		}
	}
}

const (
	configBaseDir    = "/etc/xpu"
	containerDirPerm = 0755
	configFilePerm   = 0644
	pidsSockDir      = "/var/lib/xpu"
	xpuPath          = "/opt/xpu"
)

func writeVxpuConfig(dir string, usedMem, usedCores int32) error {
	err := os.MkdirAll(dir, containerDirPerm)
	if err != nil {
		log.Errorf("mkdir vxpu config dir error: %v", err)
		return err
	}

	vxpuConfigFilePath := filepath.Clean(filepath.Join(dir, xpu.VxpuConfigFileName))
	vxpuConfig, err := os.OpenFile(vxpuConfigFilePath, os.O_WRONLY|os.O_CREATE, configFilePerm)
	if err != nil {
		log.Errorf("create vxpu config file error: %v", err)
		return err
	}
	defer vxpuConfig.Close()

	w := bufio.NewWriter(vxpuConfig)
	_, err = w.WriteString(fmt.Sprint("UsedMem:", usedMem, "\nUsedCores:", usedCores, "\n"))
	if err != nil {
		log.Errorf("bufio Writer WriteString error: %v", err)
		return err
	}
	return w.Flush()
}

// WriteVxpuIdsConfig write vxpu ids assigned to the container to vxpu-ids.config
func WriteVxpuIdsConfig(dir string, contDevs types.ContainerDevices) error {
	vxpuIdsConfigFilePath := filepath.Clean(filepath.Join(dir, xpu.VxpuIdsConfigFileName))
	vxpuIdsConfigFile, err := os.OpenFile(vxpuIdsConfigFilePath, os.O_WRONLY|os.O_CREATE, configFilePerm)
	if err != nil {
		log.Errorf("create vxpu ids config file error: %v", err)
		return err
	}
	defer vxpuIdsConfigFile.Close()

	w := bufio.NewWriter(vxpuIdsConfigFile)
	for _, contDev := range contDevs {
		_, err := w.WriteString(fmt.Sprintf("%s-%d\n", contDev.UUID, contDev.Vid))
		if err != nil {
			log.Errorf("bufio Writer WriteString error: %v", err)
			return err
		}
	}
	return w.Flush()
}

func createDirAndWriteFile(podId, containerName string, contDevs types.ContainerDevices) error {
	vxpuConfigDirInHost := filepath.Clean(filepath.Join(configBaseDir, podId, containerName))
	err := writeVxpuConfig(vxpuConfigDirInHost, contDevs[0].Usedmem, contDevs[0].Usedcores)
	if err != nil {
		log.Errorf("write vxpu config error: %v, podId: %s, containerName: %s", err, podId, containerName)
		return err
	}

	err = WriteVxpuIdsConfig(vxpuConfigDirInHost, contDevs)
	if err != nil {
		log.Errorf("write vxpu ids config error: %v, podId: %s, containerName: %s", err, podId, containerName)
		return err
	}

	return nil
}

func createContainerAllocateResponse(podId, containerName string,
	devReq types.ContainerDevices) *v1beta1.ContainerAllocateResponse {
	response := v1beta1.ContainerAllocateResponse{}
	response.Envs = make(map[string]string)
	response.Envs[xpu.VisibleDevices] = xpu.GetVisibleDevices(devReq)
	pidsSockMount := v1beta1.Mount{
		ContainerPath: filepath.Clean(pidsSockDir),
		HostPath:      filepath.Clean(pidsSockDir),
		ReadOnly:      true,
	}
	configFileMount := v1beta1.Mount{
		ContainerPath: filepath.Clean(configBaseDir),
		HostPath:      filepath.Clean(filepath.Join(configBaseDir, podId, containerName)),
		ReadOnly:      true,
	}
	xpuPathMount := v1beta1.Mount{
		ContainerPath: filepath.Clean(xpuPath),
		HostPath:      filepath.Clean(xpuPath),
		ReadOnly:      true,
	}
	response.Mounts = []*v1beta1.Mount{&pidsSockMount, &configFileMount, &xpuPathMount}
	if xpu.DevShmMount != nil {
		response.Mounts = append(response.Mounts, xpu.DevShmMount)
	}
	return &response
}

// Allocate which return list of devices.
func (m *DevicePlugin) Allocate(ctx context.Context, reqs *v1beta1.AllocateRequest) (
	*v1beta1.AllocateResponse, error) {
	log.Infoln("Allocate", reqs.ContainerRequests)
	if len(reqs.ContainerRequests) > 1 {
		return &v1beta1.AllocateResponse{}, errors.New("multiple Container Requests not supported")
	}
	responses := v1beta1.AllocateResponse{}
	nodename := config.NodeName

	current, err := util.GetPendingPod(nodename)
	if err != nil {
		lock.ReleaseNodeLock(nodename, types.VXPULockName)
		return &v1beta1.AllocateResponse{}, err
	}
	// current is nil when user pod doesn't specify vocano scheduler
	if current == nil {
		log.Errorln("user pod doesn't specify volcano scheduler")
		return &v1beta1.AllocateResponse{}, errors.New("user pod doesn't specify volcano scheduler")
	}
	log.Infoln("Allocate pod", current.Name)

	for idx := range reqs.ContainerRequests {
		curContainer, devReq, err := util.GetNextDeviceRequest(xpu.DeviceType, *current)
		if err != nil {
			log.Errorln("get device from annotation failed", err.Error())
			util.PodAllocationFailed(nodename, current)
			return &v1beta1.AllocateResponse{}, err
		}
		log.Infoln("deviceAllocateFromAnnotation=", devReq)
		if len(devReq) != len(reqs.ContainerRequests[idx].DevicesIDs) {
			log.Errorln("device number not matched", devReq, reqs.ContainerRequests[idx].DevicesIDs)
			util.PodAllocationFailed(nodename, current)
			return &v1beta1.AllocateResponse{}, errors.New("device number not matched")
		}

		err = util.EraseNextDeviceTypeFromAnnotation(xpu.DeviceType, *current)
		if err != nil {
			log.Errorln("Erase annotation failed", err.Error())
			util.PodAllocationFailed(nodename, current)
			return &v1beta1.AllocateResponse{}, err
		}

		err = createDirAndWriteFile(string(current.UID), curContainer.Name, devReq)
		if err != nil {
			log.Errorf("create dir and write file error: %v, podId: %s, containerName: %s",
				err, string(current.UID), curContainer.Name)
			return &v1beta1.AllocateResponse{}, err
		}
		response := createContainerAllocateResponse(string(current.UID), curContainer.Name, devReq)
		responses.ContainerResponses = append(responses.ContainerResponses, response)
	}
	log.Infoln("Allocate Response", responses.ContainerResponses)
	util.PodAllocationTrySuccess(nodename, current)
	return &responses, nil
}

// PreStartContainer is unimplemented for this plugin
func (m *DevicePlugin) PreStartContainer(context.Context, *v1beta1.PreStartContainerRequest) (
	*v1beta1.PreStartContainerResponse, error) {
	return &v1beta1.PreStartContainerResponse{}, nil
}

// dial establishes the gRPC communication with the registered device plugin.
func (m *DevicePlugin) dial(unixSocketPath string, timeout time.Duration) (*grpc.ClientConn, error) {
	conn, err := grpc.Dial(unixSocketPath, grpc.WithInsecure(), grpc.WithBlock(),
		grpc.WithTimeout(timeout),
		grpc.WithDialer(func(addr string, timeout time.Duration) (net.Conn, error) {
			return net.DialTimeout("unix", addr, timeout)
		}),
	)
	if err != nil {
		return nil, err
	}
	return conn, nil
}

// Devices get xpu device list from DeviceCache
func (m *DevicePlugin) Devices() []*xpu.Device {
	return m.deviceCache.GetCache()
}

func (m *DevicePlugin) apiDevices() []*v1beta1.Device {
	devices := m.Devices()
	var res []*v1beta1.Device
	for _, dev := range devices {
		for i := uint(0); i < config.DeviceSplitCount; i++ {
			id := fmt.Sprintf("%v-%v", dev.ID, i)
			res = append(res, &v1beta1.Device{
				ID:       id,
				Health:   dev.Health,
				Topology: nil,
			})
		}
	}
	return res
}
