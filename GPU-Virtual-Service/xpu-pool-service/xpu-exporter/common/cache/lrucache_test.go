/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

// Package cache implement a memory-based LRU local cache
package cache

import (
	"testing"
	"time"

	"huawei.com/xpu-exporter/common"
)

const (
	cacheTime = 100
)

func TestSetAndGet(t *testing.T) {
	cache := New(1)
	err := cache.Set("testkey1", "1", cacheTime*time.Millisecond)
	common.AssertEquals(nil, err, t)
	v, err := cache.Get("testkey1")
	common.AssertEquals("1", v, t)
	<-time.After(time.Millisecond * cacheTime)
	v, err = cache.Get("testkey1")
	common.AssertEquals(nil, v, t)
}

func TestDelete(t *testing.T) {
	cache := New(1)
	err := cache.Set("testkey1", "1", time.Duration(neverExpire))
	common.AssertEquals(nil, err, t)
	v, err := cache.Get("testkey1")
	common.AssertEquals("1", v, t)
	cache.Delete("testkey1")
	v, err = cache.Get("testkey1")
	common.AssertEquals(nil, v, t)
}

func TestIncreaseAndDecrease(t *testing.T) {
	c := &lruCache{}
	_, err := c.Increment("test", time.Minute)
	common.AssertEquals(notInitErr, err, t)
	_, err = c.Decrement("test", time.Minute)
	common.AssertEquals(notInitErr, err, t)

	cache := New(1)
	v, err := cache.IncreaseOne("testkey", time.Minute)
	common.AssertEquals(countInit, v, t)
	common.AssertEquals(nil, err, t)
	v, err = cache.IncreaseOne("testkey", time.Minute)
	common.AssertEquals(countInit+1, v, t)
	common.AssertEquals(nil, err, t)

	v, err = cache.DecreaseOne("testkey", time.Minute)
	common.AssertEquals(countInit, v, t)
	common.AssertEquals(nil, err, t)
	v, err = cache.DecreaseOne("testkey", time.Minute)
	common.AssertEquals(int64(0), v, t)
	common.AssertEquals(nil, err, t)

	// expire, will be reset to 0
	v, err = cache.DecreaseOne("testkey", time.Microsecond)
	common.AssertEquals(int64(-1), v, t)
	common.AssertEquals(nil, err, t)
	<-time.After(time.Millisecond * 1)
	v, err = cache.DecreaseOne("testkey", time.Microsecond)
	common.AssertEquals(int64(0), v, t)
	common.AssertEquals(nil, err, t)

	// decrese not exist key created
	v, err = cache.DecreaseOne("testkey1", time.Duration(neverExpire))
	common.AssertEquals(int64(0), v, t)
	common.AssertEquals(nil, err, t)
	value, err := cache.Get("testkey1")
	common.AssertEquals(int64(0), value.(int64), t)
}

func TestSetIfNotExist(t *testing.T) {
	cache := New(1)
	ret := cache.SetIfNotExist("testkey", "test", cacheTime*time.Millisecond)
	common.AssertEquals(true, ret, t)
	ret = cache.SetIfNotExist("testkey", "test", cacheTime*time.Millisecond)
	common.AssertEquals(false, ret, t)
	<-time.After(time.Millisecond * cacheTime)
	ret = cache.SetIfNotExist("testkey", "test", cacheTime*time.Millisecond)
	common.AssertEquals(true, ret, t)
}
