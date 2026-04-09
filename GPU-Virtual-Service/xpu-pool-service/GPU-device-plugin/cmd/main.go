/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

// Package main implements xpu device plugin
// 用于XPU设备资源管理和发现
package main

import (
	"flag"
	"fmt"
	"os"
	"path"
	"path/filepath"
	"syscall"

	"github.com/fsnotify/fsnotify"
	"k8s.io/kubelet/pkg/apis/deviceplugin/v1beta1"

	"huawei.com/vxpu-device-plugin/pkg/api/runtime/service"
	"huawei.com/vxpu-device-plugin/pkg/log"
	"huawei.com/vxpu-device-plugin/pkg/plugin"
	"huawei.com/vxpu-device-plugin/pkg/plugin/config"
	"huawei.com/vxpu-device-plugin/pkg/plugin/xpu"
	"huawei.com/vxpu-device-plugin/watchers"
)

const (
	xpuSockPath           = "xpu.sock"                       // XPU 设备插件的 Unix Socket 文件名
	defaultDeviceSplitNum = 2                                // 默认设备拆分数量
	defaultLogDir         = "/var/log/xpu/xpu-device-plugin" // 默认日志目录
)

var (
	resourceName string // 资源名称，通过命令行参数设置
)

func events(watcher *fsnotify.Watcher, sigs chan os.Signal, pluginInst *plugin.DevicePlugin) bool {
	for {
		select {
		case event := <-watcher.Events:
			// 监听 kubelet socket 创建事件，当 kubelet 重启时会重新创建 socket
			if event.Name == v1beta1.KubeletSocket && event.Op&fsnotify.Create == fsnotify.Create {
				log.Infof("inotify: %s created, restarting.", v1beta1.KubeletSocket)
				return true // 返回 true 触发插件重启，以重新连接 kubelet
			}

		case err := <-watcher.Errors:
			// 处理文件系统监视器的错误
			log.Infof("inotify: %s", err)

		case s := <-sigs:
			// 处理系统信号
			switch s {
			case syscall.SIGHUP:
				// SIGHUP 信号：优雅重启，重新加载配置
				log.Infoln("Received SIGHUP, restarting.")
				return true // 返回 true 触发插件重启
			default:
				// 其他信号（SIGINT、SIGTERM、SIGQUIT）：优雅关闭
				log.Infof("Received signal %v, shutting down.", s)
				pluginInst.Stop() // 停止设备插件服务
				return false      // 返回 false 退出主循环
			}
		}
	}
}

func start() error {
	// 设置文件创建权限掩码为 0，允许创建文件时有完全权限
	syscall.Umask(0)

	// 初始化日志系统，日志文件保存在配置的日志目录
	logFileName := path.Join(config.LogDir, "xpu-device-plugin.log")
	log.InitLogging(logFileName)

	// 初始化 XPU 设备发现模块，扫描系统中的 GPU/NPU 设备
	if err := xpu.Init(); err != nil {
		log.Errorf("xpu init failed: %v", err)
		return err
	}
	// 确保在函数退出时清理 XPU 资源
	defer xpu.Uninit()

	// 启动文件系统监视器，监听 kubelet socket 的创建事件
	// 当 kubelet 重启时会重新创建 socket，需要重启插件以重新连接
	log.Infof("Starting FS watcher.")
	watcher, err := watchers.NewFSWatcher(v1beta1.DevicePluginPath)
	if err != nil {
		return fmt.Errorf("failed to create FS watcher: %v", err)
	}
	defer watcher.Close() // 确保关闭文件系统监视器

	// 启动系统信号监视器，监听 SIGHUP（重启）、SIGINT/SIGTERM/SIGQUIT（关闭）信号
	log.Infof("Starting OS watcher.")
	sigs := watchers.NewOSWatcher(syscall.SIGHUP, syscall.SIGINT, syscall.SIGTERM, syscall.SIGQUIT)

	// 创建并启动设备缓存，用于缓存设备信息和状态
	cache := plugin.NewDeviceCache()
	cache.Start()
	defer cache.Stop() // 确保停止设备缓存

	// 创建并启动设备注册器，用于向 Kubernetes API Server 注册设备资源
	register := plugin.NewDeviceRegister(cache)
	register.Start()

	// 启动 PIDs 服务，提供 gRPC 服务供客户端查询进程 ID 配置
	// 这个服务会被 client/client.go 中的客户端工具调用
	service.Start()

	pluginInst := plugin.NewDevicePlugin(resourceName, cache, filepath.Clean(filepath.Join(v1beta1.DevicePluginPath, xpuSockPath)))

	// 检查是否有可用设备，如果没有设备则无法提供服务
	if len(pluginInst.Devices()) == 0 {
		return fmt.Errorf("no devices to serve for current node")
	}

	// 主循环：启动插件并处理事件
	// 当 events 函数返回 false 时（收到关闭信号），退出循环
	for {
		// 启动设备插件，向 kubelet 注册设备资源
		if err := pluginInst.Start(); err != nil {
			log.Errorf("Start vxpu device plugin failed!")
			return err
		}

		// 等待并处理事件（文件系统事件或系统信号）
		// 如果返回 true，表示需要重启插件（kubelet 重启或收到 SIGHUP）
		// 如果返回 false，表示需要关闭程序（收到关闭信号）
		if restart := events(watcher, sigs, pluginInst); !restart {
			break // 退出主循环，程序正常关闭
		}
		// 如果需要重启，循环会继续，插件会重新启动
	}
	return nil
}

func main() {
	// 定义命令行参数
	// 设备拆分数量：每个物理设备可以拆分成多个逻辑设备
	flag.UintVar(&config.DeviceSplitCount, "device-split-count", defaultDeviceSplitNum, "the number of devices to split")
	// 节点名称：从环境变量 NODE_NAME 获取，如果没有则使用默认值
	flag.StringVar(&config.NodeName, "node-name", os.Getenv("NODE_NAME"), "node name")
	// 日志目录：日志文件的存储目录
	flag.StringVar(&config.LogDir, "log-dir", defaultLogDir, "log storage directory")
	// 资源名称：Kubernetes 中的资源名称，用于向 kubelet 注册（如 "huawei.com/gpu"）
	flag.StringVar(&resourceName, "resource-name", xpu.VxpuNumber, "resource name")
	// GPU 类型配置文件：GPU 类型配置文件的绝对路径
	flag.StringVar(&config.GPUTypeConfig, "gpu-type-config", "", "the abs path map of gpu type config file")

	// 解析命令行参数
	flag.Parse()

	// 启动设备插件服务
	if err := start(); err != nil {
		// 启动失败，记录致命错误并退出程序
		log.Fatalln(err)
	}
}
