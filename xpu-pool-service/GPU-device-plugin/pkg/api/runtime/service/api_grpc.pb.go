/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

package service

import (
	context "context"

	grpc "google.golang.org/grpc"
	codes "google.golang.org/grpc/codes"
	status "google.golang.org/grpc/status"
)

const _ = grpc.SupportPackageIsVersion7

const (
	PidsService_GetPids_FullMethodName        = "/PidsService/GetPids"
	PidsService_GetAllVxpuInfo_FullMethodName = "/PidsService/GetAllVxpuInfo"
)

type PidsServiceClient interface {
	GetPids(ctx context.Context, in *GetPidsRequest, opts ...grpc.CallOption) (*GetPidsResponse, error)
	GetAllVxpuInfo(ctx context.Context, in *GetAllVxpuInfoRequest, opts ...grpc.CallOption) (*GetAllVxpuInfoResponse, error)
}

type pidsServiceClient struct {
	cc grpc.ClientConnInterface
}

func NewPidsServiceClient(cc grpc.ClientConnInterface) PidsServiceClient {
	return &pidsServiceClient{cc}
}

func (c *pidsServiceClient) GetPids(ctx context.Context, in *GetPidsRequest, opts ...grpc.CallOption) (*GetPidsResponse, error) {
	out := new(GetPidsResponse)
	err := c.cc.Invoke(ctx, PidsService_GetPids_FullMethodName, in, out, opts...)
	if err != nil {
		return nil, err
	}
	return out, nil
}

func (c *pidsServiceClient) GetAllVxpuInfo(ctx context.Context, in *GetAllVxpuInfoRequest, opts ...grpc.CallOption) (*GetAllVxpuInfoResponse, error) {
	out := new(GetAllVxpuInfoResponse)
	err := c.cc.Invoke(ctx, PidsService_GetAllVxpuInfo_FullMethodName, in, out, opts...)
	if err != nil {
		return nil, err
	}
	return out, nil
}

type PidsServiceServer interface {
	GetPids(context.Context, *GetPidsRequest) (*GetPidsResponse, error)
	GetAllVxpuInfo(context.Context, *GetAllVxpuInfoRequest) (*GetAllVxpuInfoResponse, error)
	mustEmbedUnimplementedPidsServiceServer()
}

type UnimplementedPidsServiceServer struct {
}

func (UnimplementedPidsServiceServer) GetPids(context.Context, *GetPidsRequest) (*GetPidsResponse, error) {
	return nil, status.Errorf(codes.Unimplemented, "method GetPids not implemented")
}

func (UnimplementedPidsServiceServer) GetAllVxpuInfo(context.Context, *GetAllVxpuInfoRequest) (*GetAllVxpuInfoResponse, error) {
	return nil, status.Errorf(codes.Unimplemented, "method GetAllVxpuInfo not implemented")
}

func (UnimplementedPidsServiceServer) mustEmbedUnimplementedPidsServiceServer() {}

type UnsafePidsServiceServer interface {
	mustEmbedUnimplementedPidsServiceServer()
}

func RegisterPidsServiceServer(s grpc.ServiceRegistrar, srv PidsServiceServer) {
	s.RegisterService(&PidsService_ServiceDesc, srv)
}

func _PidsService_GetPids_Handler(srv interface{}, ctx context.Context, dec func(interface{}) error, interceptor grpc.UnaryServerInterceptor) (interface{}, error) {
	in := new(GetPidsRequest)
	if err := dec(in); err != nil {
		return nil, err
	}
	if interceptor == nil {
		return srv.(PidsServiceServer).GetPids(ctx, in)
	}
	info := &grpc.UnaryServerInfo{
		Server:     srv,
		FullMethod: PidsService_GetPids_FullMethodName,
	}
	handler := func(ctx context.Context, req interface{}) (interface{}, error) {
		return srv.(PidsServiceServer).GetPids(ctx, req.(*GetPidsRequest))
	}
	return interceptor(ctx, in, info, handler)
}

func _PidsService_GetAllVxpuInfo_Handler(srv interface{}, ctx context.Context, dec func(interface{}) error, interceptor grpc.UnaryServerInterceptor) (interface{}, error) {
	in := new(GetAllVxpuInfoRequest)
	if err := dec(in); err != nil {
		return nil, err
	}
	if interceptor == nil {
		return srv.(PidsServiceServer).GetAllVxpuInfo(ctx, in)
	}
	info := &grpc.UnaryServerInfo{
		Server:     srv,
		FullMethod: PidsService_GetAllVxpuInfo_FullMethodName,
	}
	handler := func(ctx context.Context, req interface{}) (interface{}, error) {
		return srv.(PidsServiceServer).GetAllVxpuInfo(ctx, req.(*GetAllVxpuInfoRequest))
	}
	return interceptor(ctx, in, info, handler)
}

var PidsService_ServiceDesc = grpc.ServiceDesc{
	ServiceName: "PidsService",
	HandlerType: (*PidsServiceServer)(nil),
	Methods: []grpc.MethodDesc{
		{
			MethodName: "GetPids",
			Handler:    _PidsService_GetPids_Handler,
		},
		{
			MethodName: "GetAllVxpuInfo",
			Handler:    _PidsService_GetAllVxpuInfo_Handler,
		},
	},
	Streams:  []grpc.StreamDesc{},
	Metadata: "api.proto",
}
