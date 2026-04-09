/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

// Package npuservice for Prometheus
package npuservice

import (
	"time"

	"github.com/prometheus/client_golang/prometheus"

	"huawei.com/xpu-exporter/common/cache"
	"huawei.com/xpu-exporter/versions"
)

var (
	versionInfoDesc = prometheus.NewDesc(
		"npu_exporter_version_info",
		"exporter version with value '1'",
		[]string{"exporterVersion"},
		nil,
	)
)

const (
	cacheSize = 128
)

type npuCollector struct {
	cache      *cache.ConcurrencyLRUCache
	updateTime time.Duration
	cacheTime  time.Duration
}

// Describe implement the prometheus.Collector
func (n *npuCollector) Describe(ch chan<- *prometheus.Desc) {
	if ch == nil {
		return
	}
	ch <- versionInfoDesc
}

// Collect implement the prometheus.Collector
func (n *npuCollector) Collect(ch chan<- prometheus.Metric) {
	ch <- prometheus.MustNewConstMetric(versionInfoDesc, prometheus.GaugeValue, 1, []string{versions.BuildVersion}...)
}
