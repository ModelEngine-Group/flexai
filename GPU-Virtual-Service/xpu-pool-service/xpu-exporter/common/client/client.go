/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

// Package client implement grpc interface call to query vgpu information
package client

import (
	"context"
	"net"
	"time"

	"google.golang.org/grpc"
	"huawei.com/xpu-exporter/common/service"
)

const (
	pidsSockPath = "/var/lib/xpu/pids.sock"
	dialTimeout  = 5
	megabyte     = 1024 * 1024
)

// GetAllVxpuInfo Obtain vgpu information through grpc interface
func GetAllVxpuInfo() (string, error) {
	conn, err := grpc.Dial(pidsSockPath,
		grpc.WithInsecure(),
		grpc.WithBlock(),
		grpc.WithTimeout(dialTimeout*time.Second),
		grpc.WithDialer(func(addr string, timeout time.Duration) (net.Conn, error) {
			return net.DialTimeout("unix", addr, timeout)
		}),
	)
	if err != nil {
		return "", err
	}
	defer conn.Close()

	client := service.NewPidsServiceClient(conn)
	getAllVgpuInfoResponse, err := client.GetAllVxpuInfo(context.Background(), &service.GetAllVxpuInfoRequest{Period: "60"})
	if err != nil {
		return "", err
	}
	return getAllVgpuInfoResponse.VxpuInfos, nil
}
