# Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.

.PHONY: all
.DEFAULT_GOAL := all

# Compiler security options
EXTLDFLAGS := \
	-Wl,-z,relro \
	-Wl,-z,now \
	-Wl,-z,noexecstack

all: vgpu xpu-client-tool

vgpu:
	export CGO_LDFLAGS_ALLOW='-Wl,--unresolved-symbols=ignore-in-object-files' && \
	export CGO_CFLAGS='-D_FORTIFY_SOURCE=2 -O2' && \
		go build \
			-buildvcs=false \
			-tags vgpu \
			-buildmode=pie \
			-ldflags="-s -w -linkmode 'external' -extldflags '$(EXTLDFLAGS)'" \
			-o gpu-device-plugin \
			./cmd

xpu-client-tool:
	export CGO_LDFLAGS_ALLOW='-Wl,--unresolved-symbols=ignore-in-object-files' && \
	export CGO_CFLAGS='-D_FORTIFY_SOURCE=2 -O2' && \
	go build \
		-tags vgpu \
		-buildmode=pie \
		-ldflags="-s -w -linkmode 'external' -extldflags '$(EXTLDFLAGS)'" \
		-o xpu-client-tool \
		./client