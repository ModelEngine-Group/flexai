/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

// Package limiter implement a token bucket limiter
package client

import (
	"context"
	"net/http"
	"net/url"
	"testing"
	"time"

	"github.com/agiledragon/gomonkey/v2"
	"huawei.com/xpu-exporter/common"
)

const (
	maxConcur2 = 2
)

func initTestData() (*limitHandler, StatusResponseWriter, *http.Request) {
	conChan := make(chan struct{}, 1)
	lh := createHandler(conChan, http.DefaultServeMux, false, "", DefaultDataLimit)
	w := StatusResponseWriter{
		ResponseWriter: nil,
		Status:         0,
	}
	r := &http.Request{
		URL: &url.URL{
			Path: "test.com",
		},
		Header: map[string][]string{"userID": {"1"}, "reqID": {"requestIDxxxx"}},
		Method: "GET",
	}
	return lh, w, r
}

func TestServeHTTP(t *testing.T) {
	// test limitHandler serveHTTP
	h, w, r := initTestData()
	var mock = gomonkey.ApplyMethodFunc(h.httpHandler, "ServeHTTP", func(http.ResponseWriter, *http.Request) {
		return
	})
	defer mock.Reset()
	h.ServeHTTP(w.ResponseWriter, r)
	common.AssertEquals(1, len(h.concurrency), t)

	// token channel close
	mock = gomonkey.ApplyFunc(http.Error, func(http.ResponseWriter, string, int) {
		return
	})
	defer mock.Reset()
	ok := <-h.concurrency
	if !ok {
		return
	}
	h.ServeHTTP(w.ResponseWriter, r)
	common.AssertEquals(0, len(h.concurrency), t)
}

func TestReturnToken(t *testing.T) {
	mock := gomonkey.ApplyFunc(time.After, func(time.Duration) <-chan time.Time {
		tc := make(chan time.Time, 1)
		tc <- time.Time{}
		return tc
	})
	defer mock.Reset()
	cancCtx, cancelFunc := context.WithCancel(context.Background())
	defer cancelFunc()
	sc := make(chan struct{}, 1)
	go returnToken(cancCtx, sc)
	time.Sleep(time.Second)
	common.AssertEquals(1, len(sc), t)
}

func TestNewLimitHandler(t *testing.T) {
	conf := &HandlerConfig{
		PrintLog:         false,
		Method:           "",
		LimitBytes:       DefaultDataLimit,
		TotalConcurrency: defaultMaxConcurrency,
		IPConcurrency:    "2/1",
		CacheSize:        DefaultCacheSize,
	}
	_, err := NewLimitHandler(http.DefaultServeMux, conf)
	common.AssertIsNil(err, t)

	conf.IPConcurrency = "2021/1"
	_, err = NewLimitHandler(http.DefaultServeMux, conf)
	t.Log("", err)
	common.AssertNotNil(err, t)

	conf.CacheSize = 0
	_, err = NewLimitHandler(http.DefaultServeMux, conf)
	t.Log("", err)
	common.AssertNotNil(err, t)

	conf.Method = "20/iajsdkjas2jhjdklsjkljdjsdfasdl"
	_, err = NewLimitHandler(http.DefaultServeMux, conf)
	t.Log("", err)
	common.AssertNotNil(err, t)

	conf.TotalConcurrency = 0
	_, err = NewLimitHandler(http.DefaultServeMux, conf)
	t.Log("", err)
	common.AssertNotNil(err, t)
}
