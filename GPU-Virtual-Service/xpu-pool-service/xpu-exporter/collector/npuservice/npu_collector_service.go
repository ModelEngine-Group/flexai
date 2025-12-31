/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

// Package npuservice implement the gpu collector service.
package npuservice

import (
	"context"
	"time"

	"github.com/prometheus/client_golang/prometheus"

	"huawei.com/xpu-exporter/collector"
	"huawei.com/xpu-exporter/common/cache"
)

const (
	// CollectorName for npu collector
	CollectorName = "npu"
)

type npuCollectorService struct {
	serviceName string
	collector   npuCollector
}

// New create one npu collector service instance
func New(name string) collector.ICollectorService {
	return &npuCollectorService{serviceName: name}
}

// GetName obtains the service name.
func (s *npuCollectorService) GetName() string {
	return s.serviceName
}

// CreateCollector create a NPU collector instance that implements the Prometheus collector interface.
func (s *npuCollectorService) CreateCollector(cacheTime time.Duration, updateTime time.Duration) prometheus.Collector {
	s.collector = npuCollector{
		cache:      cache.New(cacheSize),
		cacheTime:  cacheTime,
		updateTime: updateTime,
	}
	return &s.collector
}

// Start start collect npu monitoring data
func (s *npuCollectorService) Start(ctx context.Context, fn context.CancelFunc) {
}
