/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

// Package client implement grpc interface call to query vgpu information
package client

import (
	"context"
	"fmt"
	"testing"

	"github.com/agiledragon/gomonkey/v2"
	"google.golang.org/grpc"
	"huawei.com/xpu-exporter/common/service"
)

type mockClient struct {
	retVal       error
	resp         *service.GetPidsResponse
	vxpuInfoResp *service.GetAllVxpuInfoResponse
}

func (mc *mockClient) GetPids(ctx context.Context, req *service.GetPidsRequest, opts ...grpc.CallOption) (*service.GetPidsResponse, error) {
	return mc.resp, mc.retVal
}

func (mc *mockClient) GetAllVxpuInfo(ctx context.Context, req *service.GetAllVxpuInfoRequest, opts ...grpc.CallOption) (*service.GetAllVxpuInfoResponse, error) {
	return mc.vxpuInfoResp, mc.retVal
}

func TestGetAllVxpuInfo(t *testing.T) {
	patchDial := gomonkey.ApplyFunc(grpc.Dial, func(target string, opts ...grpc.DialOption) (*grpc.ClientConn, error) {
		return &grpc.ClientConn{}, nil
	})

	patchNewPidsServiceClient := gomonkey.ApplyFunc(service.NewPidsServiceClient,
		func(c grpc.ClientConnInterface) service.PidsServiceClient {
			return &mockClient{retVal: fmt.Errorf("test error from GetAllVxpuInfo()")}
		})

	var c *grpc.ClientConn
	patchClose := gomonkey.ApplyMethod(c, "Close", func(_ *grpc.ClientConn) error {
		return nil
	})

	defer patchDial.Reset()
	defer patchNewPidsServiceClient.Reset()
	defer patchClose.Reset()

	_, err := GetAllVxpuInfo()
	if err == nil {
		t.Error("error in test GetAllVxpuInfo")
	} else {
		t.Log("test GetAllVxpuInfo succeed")
	}
}
