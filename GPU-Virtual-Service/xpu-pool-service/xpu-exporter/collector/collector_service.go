/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

//Package collector implement the collection service interface.

package collector

import (
	"context"
	"time"

	"github.com/prometheus/client_golang/prometheus"
)

// ICollectorService abstracts the collector service model and is compatible with various XPUs.
type ICollectorService interface {
	// CreateCollector creating a collector
	CreateCollector(cacheTime time.Duration, updateTime time.Duration) prometheus.Collector

	// Start to collect monitoring data.
	Start(ctx context.Context, fn context.CancelFunc)

	//GetName return the name of collector service
	GetName() string
}
