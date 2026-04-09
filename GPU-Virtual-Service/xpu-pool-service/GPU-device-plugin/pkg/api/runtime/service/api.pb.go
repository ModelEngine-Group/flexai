/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

package service

import (
	reflect "reflect"
	sync "sync"

	protoreflect "google.golang.org/protobuf/reflect/protoreflect"
	protoimpl "google.golang.org/protobuf/runtime/protoimpl"
)

const (
	_ = protoimpl.EnforceVersion(20 - protoimpl.MinVersion)
	_ = protoimpl.EnforceVersion(protoimpl.MaxVersion - 20)
)

type GetPidsRequest struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	CgroupPath string `protobuf:"bytes,1,opt,name=CgroupPath,proto3" json:"CgroupPath,omitempty"`
}

func (x *GetPidsRequest) Reset() {
	*x = GetPidsRequest{}
	if protoimpl.UnsafeEnabled {
		mi := &file_api_proto_msgTypes[0]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *GetPidsRequest) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*GetPidsRequest) ProtoMessage() {}

func (x *GetPidsRequest) ProtoReflect() protoreflect.Message {
	mi := &file_api_proto_msgTypes[0]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

func (*GetPidsRequest) Descriptor() ([]byte, []int) {
	return file_api_proto_rawDescGZIP(), []int{0}
}

func (x *GetPidsRequest) GetCgroupPath() string {
	if x != nil {
		return x.CgroupPath
	}
	return ""
}

type GetPidsResponse struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	EncodedPids string `protobuf:"bytes,1,opt,name=EncodedPids,proto3" json:"EncodedPids,omitempty"`
}

func (x *GetPidsResponse) Reset() {
	*x = GetPidsResponse{}
	if protoimpl.UnsafeEnabled {
		mi := &file_api_proto_msgTypes[1]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *GetPidsResponse) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*GetPidsResponse) ProtoMessage() {}

func (x *GetPidsResponse) ProtoReflect() protoreflect.Message {
	mi := &file_api_proto_msgTypes[1]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

func (*GetPidsResponse) Descriptor() ([]byte, []int) {
	return file_api_proto_rawDescGZIP(), []int{1}
}

func (x *GetPidsResponse) GetEncodedPids() string {
	if x != nil {
		return x.EncodedPids
	}
	return ""
}

type GetAllVxpuInfoRequest struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	Period string `protobuf:"bytes,1,opt,name=Period,proto3" json:"Period,omitempty"`
}

func (x *GetAllVxpuInfoRequest) Reset() {
	*x = GetAllVxpuInfoRequest{}
	if protoimpl.UnsafeEnabled {
		mi := &file_api_proto_msgTypes[2]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *GetAllVxpuInfoRequest) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*GetAllVxpuInfoRequest) ProtoMessage() {}

func (x *GetAllVxpuInfoRequest) ProtoReflect() protoreflect.Message {
	mi := &file_api_proto_msgTypes[2]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

func (*GetAllVxpuInfoRequest) Descriptor() ([]byte, []int) {
	return file_api_proto_rawDescGZIP(), []int{2}
}

func (x *GetAllVxpuInfoRequest) GetPeriod() string {
	if x != nil {
		return x.Period
	}
	return ""
}

type GetAllVxpuInfoResponse struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	VxpuInfos string `protobuf:"bytes,1,opt,name=VxpuInfos,proto3" json:"VxpuInfos,omitempty"`
}

func (x *GetAllVxpuInfoResponse) Reset() {
	*x = GetAllVxpuInfoResponse{}
	if protoimpl.UnsafeEnabled {
		mi := &file_api_proto_msgTypes[3]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *GetAllVxpuInfoResponse) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*GetAllVxpuInfoResponse) ProtoMessage() {}

func (x *GetAllVxpuInfoResponse) ProtoReflect() protoreflect.Message {
	mi := &file_api_proto_msgTypes[3]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

func (*GetAllVxpuInfoResponse) Descriptor() ([]byte, []int) {
	return file_api_proto_rawDescGZIP(), []int{3}
}

func (x *GetAllVxpuInfoResponse) GetVxpuInfos() string {
	if x != nil {
		return x.VxpuInfos
	}
	return ""
}

var File_api_proto protoreflect.FileDescriptor

var file_api_proto_rawDesc = []byte{
	0x0a, 0x09, 0x61, 0x70, 0x69, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x22, 0x30, 0x0a, 0x0e, 0x47,
	0x65, 0x74, 0x50, 0x69, 0x64, 0x73, 0x52, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x12, 0x1e, 0x0a,
	0x0a, 0x43, 0x67, 0x72, 0x6f, 0x75, 0x70, 0x50, 0x61, 0x74, 0x68, 0x18, 0x01, 0x20, 0x01, 0x28,
	0x09, 0x52, 0x0a, 0x43, 0x67, 0x72, 0x6f, 0x75, 0x70, 0x50, 0x61, 0x74, 0x68, 0x22, 0x33, 0x0a,
	0x0f, 0x47, 0x65, 0x74, 0x50, 0x69, 0x64, 0x73, 0x52, 0x65, 0x73, 0x70, 0x6f, 0x6e, 0x73, 0x65,
	0x12, 0x20, 0x0a, 0x0b, 0x45, 0x6e, 0x63, 0x6f, 0x64, 0x65, 0x64, 0x50, 0x69, 0x64, 0x73, 0x18,
	0x01, 0x20, 0x01, 0x28, 0x09, 0x52, 0x0b, 0x45, 0x6e, 0x63, 0x6f, 0x64, 0x65, 0x64, 0x50, 0x69,
	0x64, 0x73, 0x22, 0x2f, 0x0a, 0x15, 0x47, 0x65, 0x74, 0x41, 0x6c, 0x6c, 0x56, 0x78, 0x70, 0x75,
	0x49, 0x6e, 0x66, 0x6f, 0x52, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x12, 0x16, 0x0a, 0x06, 0x50,
	0x65, 0x72, 0x69, 0x6f, 0x64, 0x18, 0x01, 0x20, 0x01, 0x28, 0x09, 0x52, 0x06, 0x50, 0x65, 0x72,
	0x69, 0x6f, 0x64, 0x22, 0x36, 0x0a, 0x16, 0x47, 0x65, 0x74, 0x41, 0x6c, 0x6c, 0x56, 0x78, 0x70,
	0x75, 0x49, 0x6e, 0x66, 0x6f, 0x52, 0x65, 0x73, 0x70, 0x6f, 0x6e, 0x73, 0x65, 0x12, 0x1c, 0x0a,
	0x09, 0x56, 0x78, 0x70, 0x75, 0x49, 0x6e, 0x66, 0x6f, 0x73, 0x18, 0x01, 0x20, 0x01, 0x28, 0x09,
	0x52, 0x09, 0x56, 0x78, 0x70, 0x75, 0x49, 0x6e, 0x66, 0x6f, 0x73, 0x32, 0x82, 0x01, 0x0a, 0x0b,
	0x50, 0x69, 0x64, 0x73, 0x53, 0x65, 0x72, 0x76, 0x69, 0x63, 0x65, 0x12, 0x2e, 0x0a, 0x07, 0x47,
	0x65, 0x74, 0x50, 0x69, 0x64, 0x73, 0x12, 0x0f, 0x2e, 0x47, 0x65, 0x74, 0x50, 0x69, 0x64, 0x73,
	0x52, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x1a, 0x10, 0x2e, 0x47, 0x65, 0x74, 0x50, 0x69, 0x64,
	0x73, 0x52, 0x65, 0x73, 0x70, 0x6f, 0x6e, 0x73, 0x65, 0x22, 0x00, 0x12, 0x43, 0x0a, 0x0e, 0x47,
	0x65, 0x74, 0x41, 0x6c, 0x6c, 0x56, 0x78, 0x70, 0x75, 0x49, 0x6e, 0x66, 0x6f, 0x12, 0x16, 0x2e,
	0x47, 0x65, 0x74, 0x41, 0x6c, 0x6c, 0x56, 0x78, 0x70, 0x75, 0x49, 0x6e, 0x66, 0x6f, 0x52, 0x65,
	0x71, 0x75, 0x65, 0x73, 0x74, 0x1a, 0x17, 0x2e, 0x47, 0x65, 0x74, 0x41, 0x6c, 0x6c, 0x56, 0x78,
	0x70, 0x75, 0x49, 0x6e, 0x66, 0x6f, 0x52, 0x65, 0x73, 0x70, 0x6f, 0x6e, 0x73, 0x65, 0x22, 0x00,
	0x42, 0x0c, 0x5a, 0x0a, 0x2e, 0x2f, 0x3b, 0x73, 0x65, 0x72, 0x76, 0x69, 0x63, 0x65, 0x62, 0x06,
	0x70, 0x72, 0x6f, 0x74, 0x6f, 0x33,
}

var (
	file_api_proto_rawDescOnce sync.Once
	file_api_proto_rawDescData = file_api_proto_rawDesc
)

func file_api_proto_rawDescGZIP() []byte {
	file_api_proto_rawDescOnce.Do(func() {
		file_api_proto_rawDescData = protoimpl.X.CompressGZIP(file_api_proto_rawDescData)
	})
	return file_api_proto_rawDescData
}

var file_api_proto_msgTypes = make([]protoimpl.MessageInfo, 4)
var file_api_proto_goTypes = []any{
	(*GetPidsRequest)(nil),         // 0: server.GetPidsRequest
	(*GetPidsResponse)(nil),        // 1: server.GetPidsResponse
	(*GetAllVxpuInfoRequest)(nil),  // 2: server.GetAllVxpuInfoRequest
	(*GetAllVxpuInfoResponse)(nil), // 3: server.GetAllVxpuInfoResponse
}

var file_api_proto_depIdxs = []int32{
	0, // [0:0] is the sub-list for method output_type
	2, // [0:0] is the sub-list for method input_type
	1, // [0:0] is the sub-list for extension type_name
	3, // [0:0] is the sub-list for extension extendee
	2, // [0:0] is the sub-list for field type_name
	0,
	0,
	0,
	0,
}

func init() { file_api_proto_init() }

func file_api_proto_init() {
	if File_api_proto != nil {
		return
	}
	if !protoimpl.UnsafeEnabled {
		file_api_proto_msgTypes[0].Exporter = func(v any, i int) any {
			switch val := v.(*GetPidsRequest); i {
			case 0:
				return &val.state
			case 1:
				return &val.sizeCache
			case 2:
				return &val.unknownFields
			default:
				return nil
			}
		}
		file_api_proto_msgTypes[1].Exporter = func(v any, i int) any {
			switch v := v.(*GetPidsResponse); i {
			case 0:
				return &v.state
			case 1:
				return &v.sizeCache
			case 2:
				return &v.unknownFields
			default:
				return nil
			}
		}
		file_api_proto_msgTypes[2].Exporter = func(v any, i int) any {
			switch v := v.(*GetAllVxpuInfoRequest); i {
			case 0:
				return &v.state
			case 1:
				return &v.sizeCache
			case 2:
				return &v.unknownFields
			default:
				return nil
			}
		}
		file_api_proto_msgTypes[3].Exporter = func(v any, i int) any {
			switch val := v.(*GetAllVxpuInfoResponse); i {
			case 0:
				return &val.state
			case 1:
				return &val.sizeCache
			case 2:
				return &val.unknownFields
			default:
				return nil
			}
		}
	}
	type x struct{}
	out := protoimpl.TypeBuilder{
		File: protoimpl.DescBuilder{
			GoPackagePath: reflect.TypeOf(x{}).PkgPath(),
			RawDescriptor: file_api_proto_rawDesc,
			NumEnums:      0,
			NumMessages:   4,
			NumExtensions: 0,
			NumServices:   1,
		},
		GoTypes:           file_api_proto_goTypes,
		DependencyIndexes: file_api_proto_depIdxs,
		MessageInfos:      file_api_proto_msgTypes,
	}.Build()
	File_api_proto = out.File
	file_api_proto_goTypes = nil
	file_api_proto_depIdxs = nil
	file_api_proto_rawDesc = nil
}
