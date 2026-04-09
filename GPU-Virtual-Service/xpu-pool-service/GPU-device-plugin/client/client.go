/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

// Package main implements xpu client tool
// 用于 Kubernetes GPU 设备插件场景，根据 cgroup 路径查询/更新相关进程 ID 配置，用于资源管理和监控
package main

import (
	"context"
	"flag"
	"fmt"
	"net"
	"os"
	"time"

	"google.golang.org/grpc"

	"huawei.com/vxpu-device-plugin/pkg/api/runtime/service"
	"huawei.com/vxpu-device-plugin/pkg/log"
)

const (
	pidsSockPath = "/var/lib/xpu/pids.sock"
	dialTimeout  = 5
)

func updatePidsConfig(cgroupPath string) error {
	// 根据配置的 Unix Socket 建立到 pids service 的 gRPC 连接
	conn, err := grpc.Dial(
		pidsSockPath,
		grpc.WithInsecure(),
		grpc.WithBlock(),
		grpc.WithTimeout(dialTimeout*time.Second),
		grpc.WithDialer(
			func(addr string, timeout time.Duration) (net.Conn, error) {
				// 使用 Unix Domain Socket 进行拨号，确保超时控制
				return net.DialTimeout("unix", addr, timeout)
			}),
	)
	if err != nil {
		// 连接建立失败，记录日志并返回错误
		log.Errorf("grpc dial error: %v", err)
		return err
	}
	if conn == nil {
		// 防御式检查，避免 nil 连接导致后续调用 panic
		return fmt.Errorf("client connection is nil")
	}
	defer conn.Close()

	//根据 cgroup 路径查询/更新相关进程 ID 配置
	client := service.NewPidsServiceClient(conn)
	_, err = client.GetPids(context.Background(), &service.GetPidsRequest{CgroupPath: cgroupPath})
	if err != nil {
		log.Errorf("client GetPids error: %v", err)
		return err
	}
	return nil
}

func main() {
	// 通过命令行参数获取目标 cgroup 路径
	var cgroupPath string
	flag.StringVar(&cgroupPath, "cgroup-path", "", "cgroup path")
	flag.Parse()

	// 调用 gRPC 客户端根据 cgroup 路径同步 PID 配置
	err := updatePidsConfig(cgroupPath)
	if err != nil {
		// 请求失败时打印错误并退出进程
		log.Errorf("get pids failed, cgroupPat:%s", cgroupPath)
		os.Exit(1)
	}
}
