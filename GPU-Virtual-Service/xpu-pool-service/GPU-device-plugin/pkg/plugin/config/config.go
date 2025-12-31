/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

// Package config defines configure for vxpu device plugin
package config

var (
	// DeviceSplitCount count of vxpu split from a physical xpu
	DeviceSplitCount uint
	// NodeName current node name
	NodeName string
	// LogDir log storage directory
	LogDir string
	// GPUTypeConfig The absolute path of gpu type config file
	GPUTypeConfig string
	// GPUTypeMap mapping between gpu types and abbreviations
	GPUTypeMap map[string]string
)
