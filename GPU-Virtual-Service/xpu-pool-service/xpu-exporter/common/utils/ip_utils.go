/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

// Package utils offer some utils for certificate handling
package utils

import (
	"net"
	"net/http"
	"strings"
)

// ClientIP try to get the clientIP
func ClientIP(r *http.Request) string {
	// get forwarded ip firstly
	var ip string
	xForwardedFor := r.Header.Get("X-Forwarded-For")
	forwardSlice := strings.Split(xForwardedFor, ",")
	if len(forwardSlice) >= 1 {
		if ip = strings.TrimSpace(forwardSlice[0]); ip != "" {
			return ip
		}
	}

	// try get ip from "X-Real-Ip"
	ip = strings.TrimSpace(r.Header.Get("X-Real-Ip"))
	if ip != "" {
		return ip
	}

	var err error
	if ip, _, err = net.SplitHostPort(strings.TrimSpace(r.RemoteAddr)); err == nil {
		return ip
	}

	return ""
}
