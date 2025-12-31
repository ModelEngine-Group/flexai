/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

// Package main
// XPU Exporter 是一个 Prometheus 指标导出器，用于收集和暴露 GPU/NPU 设备的监控指标
package main

import (
	"context"
	"errors"
	"flag"
	"fmt"
	"os"
	"path"
	"sync"
	"syscall"
	"time"

	"github.com/prometheus/client_golang/prometheus"
	"huawei.com/vxpu-device-plugin/pkg/log"
	"huawei.com/xpu-exporter/collector/gpuservice"
	"huawei.com/xpu-exporter/collector/npuservice"
	"huawei.com/xpu-exporter/server"
	"huawei.com/xpu-exporter/versions"
)

var (
	updateTime int
	xpuType    string
)

const (
	exporterServerPort = 8082                        // 默认 HTTP 服务端口
	updateTimeConst    = 5                           // 默认更新间隔（秒）
	oneMinute          = 60                          // 一分钟的秒数
	cacheTime          = 65 * time.Second            // 缓存过期时间
	defaultConcurrency = 5                           // 默认最大并发数
	defaultConnection  = 20                          // 默认连接数限制
	defaultLogDir      = "/var/log/xpu/xpu-exporter" // 默认日志目录
)

var serverHandler *server.ExporterServer

// init 函数在程序启动时初始化命令行参数
func init() {
	serverHandler = &server.ExporterServer{}
	// 定义命令行参数
	flag.StringVar(&xpuType, "type", "", "Set xpu type,range[gpu,npu],can not be empty")
	flag.IntVar(&updateTime, "updateTime", updateTimeConst,
		"Interval (seconds) to update the npu metric cache,range[1-60]")
	flag.IntVar(&serverHandler.Port, "port", exporterServerPort,
		"The serverHandler port of the http service, range[1025-40000]")
	flag.StringVar(&serverHandler.Ip, "ip", os.Getenv("HOST_IP"),
		"The listen ip of the service,0.0.0.0 is not recommended when install on Multi-NIC host")
	flag.IntVar(&serverHandler.Concurrency, "concurrency", defaultConcurrency,
		"The max concurrency of the http serverHandler, range is [1-512]")
	flag.IntVar(&serverHandler.LimitIPConn, "limitIPConn", defaultConcurrency,
		"the tcp connection limit for each Ip, range is [1,128]")
	flag.IntVar(&serverHandler.LimitTotalConn, "limitTotalConn", defaultConnection,
		"the tcp connection limit for all request, range is [1,512]")
	flag.StringVar(&serverHandler.LimitIPReq, "limitIPReq", "20/1",
		"the http request limit counts for each Ip,20/1 means allow 20 request in 1 seconds")
}

// checkCommonParamValid 检查通用参数的有效性
func checkCommonParamValid() error {
	// 验证更新间隔是否在有效范围内（1-60秒）
	if updateTime > oneMinute || updateTime < 1 {
		return errors.New("the updateTime is invalid")
	}
	return nil
}

// loadCollectorService 根据 XPU 类型加载相应的收集器服务
func loadCollectorService() error {
	var err error
	switch xpuType {
	case npuservice.CollectorName:
		// 加载 NPU 收集器服务
		err = serverHandler.RegisterCollectorService(npuservice.New(npuservice.CollectorName))
	case gpuservice.CollectorName:
		// 加载 GPU 收集器服务
		err = serverHandler.RegisterCollectorService(gpuservice.New(gpuservice.CollectorName))
	default:
		// XPU 类型参数缺失或无效
		err = fmt.Errorf("the mandatory parameter type=npu or type=gpu is missing or value[%s] error", xpuType)
	}
	return err
}

func main() {
	flag.Parse()

	syscall.Umask(0)
	logFileName := path.Join(defaultLogDir, "xpu-exporter.log")
	log.InitLogging(logFileName)

	log.Infof("npu exporter starting and the version is %s", versions.BuildVersion)
	if err := loadCollectorService(); err != nil {
		log.Fatalln(err)
	}

	if err := checkCommonParamValid(); err != nil {
		log.Fatalln(err)
	}

	if err := serverHandler.VerifyServerParams(); err != nil {
		log.Fatalln(err)
	}

	c := serverHandler.CreateCollector(cacheTime, time.Duration(updateTime)*time.Second)
	reg := prometheus.NewRegistry()
	reg.MustRegister(c)

	ctx, cancel := context.WithCancel(context.Background())
	wg := &sync.WaitGroup{}
	wg.Add(1)
	go func() {
		defer wg.Done()
		serverHandler.StartCollect(ctx, cancel)
	}()
	wg.Add(1)
	go func() {
		defer wg.Done()
		serverHandler.StartServe(ctx, cancel, reg)
	}()
	wg.Wait()
}
