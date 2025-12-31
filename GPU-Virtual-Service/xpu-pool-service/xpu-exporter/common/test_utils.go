/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

// Package common implement some test utilities.
package common

import (
	"fmt"
	"path/filepath"
	"reflect"
	"runtime"
	"testing"
)

// AssertEquals assert equals
func AssertEquals(expect, act interface{}, t testing.TB) {
	equals(expect, act, t, 1)
}

// AssertIsNil assert is nil
func AssertIsNil(obtained interface{}, t testing.TB) {
	isNil(obtained, t, 1)
}

// AssertNotNil assert not nil
func AssertNotNil(obtained interface{}, t testing.TB) {
	notNil(obtained, t, 1)
}

func equals(expect, act interface{}, t testing.TB, caller int) {
	if !reflect.DeepEqual(expect, act) {
		_, file, line, _ := runtime.Caller(caller + 1)
		fmt.Printf("%s:%d: expect: %v (%T), got: %v (%T)\n",
			filepath.Base(file), line, expect, expect, act, act)
		t.FailNow()
	}
}

func isNil(obj interface{}, t testing.TB, caller int) {
	if !_isNil(obj) {
		_, file, line, _ := runtime.Caller(caller + 1)
		fmt.Printf("%s:%d: expected nil, got: %v\n", filepath.Base(file), line, obj)
		t.FailNow()
	}
}

func notNil(obj interface{}, t testing.TB, caller int) {
	if _isNil(obj) {
		_, file, line, _ := runtime.Caller(caller + 1)
		fmt.Printf("%s:%d: expected non-nil, got: %v\n", filepath.Base(file), line, obj)
		t.FailNow()
	}
}

func _isNil(obj interface{}) bool {
	if obj == nil {
		return true
	}

	switch v := reflect.ValueOf(obj); v.Kind() {
	case reflect.Chan, reflect.Func, reflect.Interface, reflect.Map, reflect.Ptr, reflect.Slice:
		return v.IsNil()
	default:
		return false
	}
}
