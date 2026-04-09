/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

// Package gpuservice implement gpu collecion service interface.
package gpuservice

import (
	"context"
	"sync"
	"time"

	"github.com/prometheus/client_golang/prometheus"
	"huawei.com/xpu-exporter/collector"
	"huawei.com/xpu-exporter/common/cache"
	"huawei.com/xpu-exporter/common/client"
)

const (
	// CollectorName for gpu collector
	CollectorName       = "gpu"
	vgpuInfoCacheKey    = "xpu-exporter-vgpu-info"
	updateCachePattern  = "update cache,key is %s"
	tickerFailedPattern = "%s ticker failed, task shutdown"
)

type gpuCollectorService struct {
	serviceName string
	collector   gpuCollector
}

// New create one gpu collector service instance
func New(name string) collector.ICollectorService {
	return &gpuCollectorService{serviceName: name}
}

// GetName obtains the service name.
func (s *gpuCollectorService) GetName() string {
	return s.serviceName
}

// CreateCollector create a GPU collector instance that implements the Prometheus collector interface.
func (s *gpuCollectorService) CreateCollector(cacheTime time.Duration, updateTime time.Duration) prometheus.Collector {
	s.collector = gpuCollector{
		cache:      cache.New(cacheSize),
		cacheTime:  cacheTime,
		updateTime: updateTime,
	}
	return &s.collector
}

// Start start collect gpu monitoring data
func (s *gpuCollectorService) Start(ctx context.Context, fn context.CancelFunc) {
	group := &sync.WaitGroup{}
	vgpuInfoCollect(ctx, group, &s.collector)
	group.Wait()
}

func vgpuInfoCollect(ctx context.Context, group *sync.WaitGroup, n *gpuCollector) {
	group.Add(1)
	go func() {
		setVgpuInfoToCache(ctx, group, n)
	}()
}

func setVgpuInfoToCache(ctx context.Context, group *sync.WaitGroup, n *gpuCollector) {
	defer group.Done()
	ticker := time.NewTicker(n.updateTime)
	defer ticker.Stop()

	for {
		select {
		case <-ctx.Done():
			return
		default:
			_, err := client.GetAllVxpuInfo()
			if err != nil {
				return
			}
			if _, ok := <-ticker.C; !ok {
				return
			}
		}
	}
}
