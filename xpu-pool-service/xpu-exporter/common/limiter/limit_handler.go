/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

// Package limiter implement a token bucket limiter
package limiter

import (
	"context"
	"errors"
	"fmt"
	"math"
	"net/http"
	"regexp"
	"strconv"
	"strings"
	"sync"
	"syscall"
	"time"

	"huawei.com/xpu-exporter/common/cache"
	"huawei.com/xpu-exporter/common/utils"
)

const (
	kilo = 1000.0
	// DefaultDataLimit default http body limit size
	DefaultDataLimit      = 1024 * 1024 * 10
	defaultMaxConcurrency = 1024
	httpMaxWaitTime       = 5
	maxStringLen          = 20
	// DefaultCacheSize default cache size
	DefaultCacheSize = 1024 * 100
	arrLen           = 2
	IPReqLimitReg    = "^[1-9]\\d{0,2}/[1-9]\\d{0,2}$"
	// MaxIPRequestsPerMinute defines the maximum number of requests allowed per IP per minute
	MaxIPRequestsPerMinute = 20
)

type limitHandler struct {
	concurrency       chan struct{}
	httpHandler       http.Handler
	log               bool
	method            string
	limitBytes        int64
	ipExpiredTime     time.Duration
	ipCache           *cache.ConcurrencyLRUCache
	ipRequestCounters map[string]*ipRequestCounter
	mu                sync.Mutex
}

type ipRequestCounter struct {
	requests int
	timer    *time.Timer
}

type HandlerConfig struct {
	// PrintLog whether you need print access log, when use gin framework, suggest to set false, otherwise set true
	PrintLog bool
	// Method only allow setting http method pass
	Method string
	// LimitBytes set the max http body size
	LimitBytes int64
	// TotalConCurrency set the program total concurrent http request
	TotalConCurrency int
	// IPConCurrency set the single IP concurrent http request "2/1sec"
	IPConCurrency string
	// CacheSize set the local cache size
	CacheSize int
}

type StatusResponseWriter struct {
	http.ResponseWriter
	http.Hijacker
	Status int
}

func (w *StatusResponseWriter) WriteHeader(status int) {
	w.ResponseWriter.WriteHeader(status)
	w.Status = status
}

func (h *limitHandler) checkIPRequestLimit(clientIP, path string, req *http.Request) bool {
	h.mu.Lock()
	defer h.mu.Unlock()

	counter, exists := h.ipRequestCounters[clientIP]
	if !exists {
		counter = &ipRequestCounter{
			requests: 0,
			timer: time.AfterFunc(time.Minute, func() {
				h.mu.Lock()
				delete(h.ipRequestCounters, clientIP)
				h.mu.Unlock()
			}),
		}
		h.ipRequestCounters[clientIP] = counter
	}

	if counter.requests >= MaxIPRequestsPerMinute {
		return false
	}
	counter.requests++
	return true
}

func (h *limitHandler) ServeHTTP(w http.ResponseWriter, req *http.Request) {
	req.Body = http.MaxBytesReader(w, req.Body, h.limitBytes)
	ctx := initContext(req)
	path := req.URL.Path
	clientUserAgent := req.UserAgent()
	clientIP := utils.ClientIP(req)

	// Check if the IP has exceeded the limit of 20 requests per minute
	if !h.checkIPRequestLimit(clientIP, path, req) {
		http.Error(w, "429 Too Many Requests", http.StatusTooManyRequests)
		return
	}

	if clientIP != "" && h.ipCache != nil {
		if !h.ipCache.SetIfNotExist(fmt.Sprintf("key-%s", clientIP), "v", h.ipExpiredTime) {
			http.Error(w, "503 too busy", http.StatusServiceUnavailable)
			return
		}
	}

	select {
	case _, ok := <-h.concurrency:
		if !ok {
			// The channel is closed. The token does not need to be returned.
			return
		}
		if h.method != "" && req.Method != h.method {
			http.NotFound(w, req)
			h.concurrency <- struct{}{} // recover token to the bucket
			return
		}
		cancCtx, cancelFunc := context.WithCancel(ctx)
		start := time.Now()
		go returnToken(cancCtx, h.concurrency)
		statusRes := newResponse(w)
		h.httpHandler.ServeHTTP(statusRes, req)
		stop := time.Since(start)
		cancelFunc()
		if stop < httpMaxWaitTime*time.Second {
			h.concurrency <- struct{}{}
		}
	default:
		http.Error(w, "503 too busy", http.StatusServiceUnavailable)
	}
}

func newResponse(w http.ResponseWriter) *StatusResponseWriter {
	jk, _ := w.(http.Hijacker)

	statusRes := &StatusResponseWriter{
		ResponseWriter: w,
		Status:         http.StatusOK,
		Hijacker:       jk,
	}
	return statusRes
}

// initContext returns a context for the request.
func initContext(req *http.Request) context.Context {
	return context.Background()
}

// returnToken returns the token back to the concurrency channel after timeout or cancellation.
func returnToken(ctx context.Context, concurrency chan struct{}) {
	// If a request is processed for more than 5s(httpMaxWaitTime),
	// the token must be returned to prevent the HTTP service from being unavailable.
	timeAfterTrigger := time.After(time.Second * httpMaxWaitTime)
	if concurrency == nil || timeAfterTrigger == nil {
		return
	}

	for {
		select {
		case _, ok := <-timeAfterTrigger:
			if !ok {
				return
			}
			concurrency <- struct{}{}
			return
		case _, _ = <-ctx.Done():
			return
		}
	}
}

func createHandler(ch chan struct{}, handler http.Handler, printLog bool, httpMethod string, bodySizeLimit int64) *limitHandler {
	h := &limitHandler{
		concurrency:       ch,
		httpHandler:       handler,
		log:               printLog,
		method:            httpMethod,
		limitBytes:        bodySizeLimit,
		ipExpiredTime:     time.Duration(-1),
		ipRequestCounters: make(map[string]*ipRequestCounter),
	}
	for i := 0; i < cap(ch); i++ {
		h.concurrency <- struct{}{}
	}
	return h
}

func NewLimitHandler(handler http.Handler, conf *HandlerConfig) (http.Handler, error) {
	if conf == nil {
		return nil, errors.New("parameter error")
	}
	if conf.TotalConCurrency < 1 || conf.TotalConCurrency > defaultMaxConcurrency {
		return nil, errors.New("totalConCurrency parameter error")
	}
	if len(conf.Method) > maxStringLen {
		return nil, errors.New("http method error")
	}
	if conf.CacheSize <= 0 {
		conf.CacheSize = DefaultCacheSize
	}

	// To verify the validity of the configuration of the number of concurrent requests sent from a single IP address per second.
	reg := regexp.MustCompile(IPReqLimitReg)
	if !reg.Match([]byte(conf.IPConCurrency)) {
		return nil, errors.New("IPConCurrency parameter error")
	}

	conChan := make(chan struct{}, conf.TotalConCurrency)
	h := createHandler(conChan, handler, conf.PrintLog, conf.Method, conf.LimitBytes)

	arr := strings.Split(conf.IPConCurrency, "/")
	if len(arr) != arrLen || arr[0] == "0" {
		return nil, errors.New("IPConCurrency parameter error")
	}

	arr1, err := strconv.ParseInt(arr[1], 0, 0)
	if err != nil {
		return nil, fmt.Errorf("IPConCurrency parameter(%s) error, parse to int failed: %v", arr[1], err)
	}
	arr0, err := strconv.ParseInt(arr[0], 0, 0)
	if err != nil || arr0 == 0 {
		return nil, fmt.Errorf("IPConCurrency parameter(%s) error, parse to int failed: %v", arr[0], err)
	}
	h.ipExpiredTime = time.Duration(arr1 * int64(time.Second) / arr0)

	return h, nil
}
