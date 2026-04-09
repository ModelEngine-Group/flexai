/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

// Package cache implement a memory-based LRU local cache
package cache

import (
	"container/list"
	"errors"
	"fmt"
	"math"
	"sync"
	"time"
)

const (
	segmentCount        = 16
	tenYears            = 10 * 365 * 24 * time.Hour
	hashInit     uint32 = 2654435761 // a prime number close to 2^29.
	prime32      uint32 = 16777619   // a common 32-bit prime number
	neverExpire  int64  = -1
	countInit    int64  = 1
)

var (
	notInitErr = errors.New("lru cache not init")
	paraErr    = errors.New("parameter error")
)

type cacheElement struct {
	key        string
	data       interface{}
	expireTime int64
}

type lruCache struct {
	maxSize   int
	elemIndex map[string]*list.Element
	*list.List
	mu sync.Mutex
}

// ConcurrencyLRUCache is a memory-based LRU local cache, to improve the concurrent performance,
// the data is divided into 16 segments by default.
type ConcurrencyLRUCache struct {
	segment     int
	cacheBucket [segmentCount]*lruCache
}

// index calculate by the key
func (cl *ConcurrencyLRUCache) index(key string) int {
	var hash = hashInit
	for i := 0; i < len(key); i++ {
		hash *= prime32
		hash ^= uint32(key[i])
	}
	return int(hash & (uint32(cl.segment) - 1))
}

// Set create or update an element using key. Expire time:-1 means never overdue,unit: nanosecond
func (cl *ConcurrencyLRUCache) Set(key string, value interface{}, expireTime time.Duration) error {
	if cl == nil || cl.cacheBucket[0] == nil {
		return notInitErr
	}
	if expireTime < time.Duration(neverExpire) || expireTime > tenYears {
		return paraErr
	}
	cacheIndex := cl.index(key)
	if cacheIndex < 0 || cacheIndex >= segmentCount {
		return errors.New("index out of valid value")
	}
	return cl.cacheBucket[cacheIndex].setValue(key, value, expireTime)
}

// Get the value of a cached element by key. If key do not exist, return nil and an error msg
func (cl *ConcurrencyLRUCache) Get(key string) (interface{}, error) {
	if cl == nil || cl.cacheBucket[0] == nil {
		return nil, notInitErr
	}
	cacheIndex := cl.index(key)
	if cacheIndex < 0 || cacheIndex >= segmentCount {
		return nil, errors.New("index out of valid value")
	}
	return cl.cacheBucket[cacheIndex].getValue(key)
}

// Delete delete the value by key, no error returned
func (cl *ConcurrencyLRUCache) Delete(key string) {
	if cl == nil || cl.cacheBucket[0] == nil {
		return
	}
	cacheIndex := cl.index(key)
	if cacheIndex < 0 || cacheIndex >= segmentCount {
		return
	}
	cl.cacheBucket[cacheIndex].delValue(key)
}

// SetIfNotExist if the key not exist or expired, set the new value to cache and return true, otherwise return false
func (cl *ConcurrencyLRUCache) SetIfNotExist(key string, value interface{}, expireTime time.Duration) bool {
	if cl == nil || cl.cacheBucket[0] == nil {
		return false
	}
	if expireTime < time.Duration(neverExpire) || expireTime > tenYears {
		return false
	}
	cacheIndex := cl.index(key)
	if cacheIndex < 0 || cacheIndex >= segmentCount {
		return false
	}
	return cl.cacheBucket[cacheIndex].setIfNotExist(key, value, expireTime)
}

// IncreaseOne add one to value of the key ,if the key not exist or expire, initialize with 0 and then add one
// if the key not exist, but value not int64, return err
func (cl *ConcurrencyLRUCache) IncreaseOne(key string, expireTime time.Duration) (int64, error) {
	if err := validate(cl, expireTime); err != nil {
		return 0, err
	}
	cacheIndex := cl.index(key)
	if cacheIndex < 0 || cacheIndex >= segmentCount {
		return 0, errors.New("index out of valid value")
	}
	return cl.cacheBucket[cacheIndex].increment(key, expireTime)
}

// DecreaseOne minus one to the value of the key ,if the key not exist, initialize with 0
func (cl *ConcurrencyLRUCache) DecreaseOne(key string, expireTime time.Duration) (int64, error) {
	if err := validate(cl, expireTime); err != nil {
		return 0, err
	}
	cacheIndex := cl.index(key)
	if cacheIndex < 0 || cacheIndex >= segmentCount {
		return 0, errors.New("index out of valid value")
	}
	return cl.cacheBucket[cacheIndex].decrement(key, expireTime)
}

func validate(cl *ConcurrencyLRUCache, expireTime time.Duration) error {
	if cl == nil || cl.cacheBucket[0] == nil {
		return paraErr
	}
	if expireTime <= 0 && expireTime != time.Duration(neverExpire) {
		return paraErr
	}
	return nil
}

// New create an instance of ConcurrencyLRUCache
func New(maxEntry int) *ConcurrencyLRUCache {
	if maxEntry <= 0 {
		return nil
	}
	size := (maxEntry + segmentCount - 1) / segmentCount
	var cache [segmentCount]*lruCache
	for i := 0; i < segmentCount; i++ {
		cache[i] = &lruCache{
			maxSize:   size,
			elemIndex: make(map[string]*list.Element, segmentCount),
			List:      list.New(),
			mu:        sync.Mutex{},
		}
	}
	return &ConcurrencyLRUCache{
		segment:     segmentCount,
		cacheBucket: cache,
	}
}

func (c *lruCache) setValue(key string, value interface{}, expireTime time.Duration) error {
	if c == nil || c.elemIndex == nil {
		return errors.New("not initialized")
	}
	c.mu.Lock()
	defer c.mu.Unlock()
	v, ok := c.elemIndex[key]
	if !ok {
		// if the cache not exist
		c.setInner(key, value, expireTime)
		return nil
	}
	element, ok := v.Value.(*cacheElement)
	if !ok {
		c.safeDeleteByKey(key, v)
		return errors.New("cacheElement convert failed")
	}
	c.MoveToFront(v)
	paddingElement(element, value, expireTime)
	return nil
}

func (c *lruCache) getValue(key string) (interface{}, error) {
	if c == nil || c.elemIndex == nil {
		return nil, errors.New("not initialized")
	}
	c.mu.Lock()
	defer c.mu.Unlock()
	v, ok := c.elemIndex[key]
	if !ok {
		return nil, errors.New("no value found")
	}
	c.MoveToFront(v)
	ele, ok := v.Value.(*cacheElement)
	if !ok {
		c.safeDeleteByKey(key, v)
		return nil, errors.New("cacheElement convert failed")
	}
	if ele.expireTime != neverExpire && time.Now().UnixNano() > ele.expireTime {
		// if cache expired
		c.safeDeleteByKey(key, v)
		return nil, errors.New("the key was expired")
	}
	return ele.data, nil
}

func (c *lruCache) delValue(key string) {
	if c == nil || c.elemIndex == nil {
		return
	}
	c.mu.Lock()
	defer c.mu.Unlock()
	if v, ok := c.elemIndex[key]; ok {
		c.safeDeleteByKey(key, v)
	}
}

func (c *lruCache) increment(key string, expireTime time.Duration) (int64, error) {
	if c == nil || c.elemIndex == nil {
		return 0, notInitErr
	}
	c.mu.Lock()
	defer c.mu.Unlock()
	v, ok := c.elemIndex[key]
	if !ok {
		c.setInner(key, countInit, expireTime)
		return countInit, nil
	}
	element, ok := v.Value.(*cacheElement)
	if !ok {
		c.safeDeleteByKey(key, v)
		c.setInner(key, countInit, expireTime)
		return countInit, nil
	}
	c.MoveToFront(v)
	if element.expireTime == neverExpire || time.Now().UnixNano() < element.expireTime {
		newValue, ok := element.data.(int64)
		if !ok || newValue == math.MaxInt64 {
			return 0, fmt.Errorf("the cache value is not valid, ok:%v", ok)
		}
		newValue++
		paddingElement(element, newValue, expireTime)
		return newValue, nil
	}
	// if cache expired
	paddingElement(element, countInit, expireTime)
	return countInit, nil
}

func (c *lruCache) decrement(key string, expireTime time.Duration) (int64, error) {
	if c == nil || c.elemIndex == nil {
		return 0, notInitErr
	}
	c.mu.Lock()
	defer c.mu.Unlock()
	v, ok := c.elemIndex[key]
	if !ok {
		c.setInner(key, int64(0), expireTime)
		return 0, nil
	}
	element, ok := v.Value.(*cacheElement)
	if !ok {
		c.safeDeleteByKey(key, v)
		c.setInner(key, int64(0), expireTime)
		return 0, nil
	}
	c.MoveToFront(v)
	if element.expireTime == neverExpire || time.Now().UnixNano() < element.expireTime {
		newValue, ok := element.data.(int64)
		if !ok || newValue == math.MinInt64 {
			return 0, fmt.Errorf("the cache value is not valid, ok:%v", ok)
		}
		newValue--
		paddingElement(element, newValue, expireTime)
		return newValue, nil
	}
	// if cache expired
	paddingElement(element, int64(0), expireTime)
	return 0, nil
}

func (c *lruCache) setIfNotExist(key string, value interface{}, expireTime time.Duration) bool {
	if c == nil || c.elemIndex == nil {
		return false
	}
	c.mu.Lock()
	defer c.mu.Unlock()
	v, ok := c.elemIndex[key]
	if !ok {
		// if the cache not exist
		c.setInner(key, value, expireTime)
		return true
	}
	ele, ok := v.Value.(*cacheElement)
	if !ok {
		c.safeDeleteByKey(key, v)
		return false
	}
	c.MoveToFront(v)
	if ele.expireTime == neverExpire || time.Now().UnixNano() < ele.expireTime {
		return false
	}
	// if cache expired
	paddingElement(ele, value, expireTime)
	return true
}

func paddingElement(element *cacheElement, value interface{}, expireTime time.Duration) {
	element.data = value
	if expireTime == time.Duration(neverExpire) {
		element.expireTime = neverExpire
		return
	}
	element.expireTime = time.Now().UnixNano() + int64(expireTime)
}

func (c *lruCache) setInner(key string, value interface{}, expireTime time.Duration) {
	if c == nil {
		return
	}
	if c.Len()+1 > c.maxSize {
		c.safeRemoveOldest()
	}
	newElem := &cacheElement{
		key:        key,
		data:       value,
		expireTime: neverExpire,
	}
	if expireTime != time.Duration(neverExpire) {
		newElem.expireTime = time.Now().UnixNano() + int64(expireTime)
	}
	e := c.PushFront(newElem)
	c.elemIndex[key] = e
}

func (c *lruCache) safeDeleteByKey(key string, v *list.Element) {
	if c == nil {
		return
	}
	c.List.Remove(v)
	delete(c.elemIndex, key)
}

func (c *lruCache) safeRemoveOldest() {
	if c == nil {
		return
	}
	v := c.List.Back()
	if v == nil {
		return
	}
	c.List.Remove(v)
	ele, ok := v.Value.(*cacheElement)
	if !ok {
		return
	}
	delete(c.elemIndex, ele.key)
}
