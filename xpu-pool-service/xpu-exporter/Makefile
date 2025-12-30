# Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.

.PHONY: all
.DEFAULT_GOAL := all
OUTPUT_NAME := "xpu-exporter"
build_version := "v1.0.RC2"

# Compiler security options
EXTLDFLAGS := \
	-Wl,-z,relro \
	-Wl,-z,now \
	-Wl,-z,noexecstack

all: xpu-exporter

xpu-exporter:
	export CGO_LDFLAGS_ALLOW='-Wl,--unresolved-symbols=ignore-in-object-files' && \
	go build \
		-buildmode=pie \
		-ldflags="-s -w -linkmode 'external' -extldflags '$(EXTLDFLAGS)' \
		-X huawei.com/npu-exporter/v6/versions.BuildName=${OUTPUT_NAME} \
		-X huawei.com/xpu-exporter/versions.BuildVersion=${build_version}" \
		-o xpu-exporter \
		./cmd/xpu-exporter

clean:
	rm -rf ./xpu-exporter