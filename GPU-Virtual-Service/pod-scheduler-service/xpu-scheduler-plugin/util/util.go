/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

// Package util defines data structure and provide util function for xpu scheduler plugin implementation
package util

import (
	"errors"
	"fmt"
	"strconv"
	"strings"

	"k8s.io/api/core/v1"
)

// IsXPUName determine whether it is the xpu name
func IsXPUName(s string) bool {
	return s == VGPUName || s == VNPUName
}

// IsXPUTopologyIntraBandwidth determine whether the topology intra bandwidth is configured
func IsXPUTopologyIntraBandwidth(s string) bool {
	if s == XPUTopologyIntraBandwidthAnnotation {
		return true
	}
	return false
}

// SafePrint safe print error
func SafePrint(args ...interface{}) string {
	msg := fmt.Sprint(args...)
	trimMsg := strings.Replace(msg, "\r", " ", -1)
	trimMsg = strings.Replace(trimMsg, "\n", " ", -1)
	return trimMsg
}

// GetVXPUResource for get vxpu resource from container Limits
func GetVXPUResource(container *v1.Container, resourceName string) int {
	resourceNum, ok := container.Resources.Limits[v1.ResourceName(resourceName)]
	if ok {
		return int(resourceNum.Value())
	}
	return 0
}

// GetXPUType get the xpu device type if configured
func GetXPUType(container *v1.Container, xpuType string) string {
	for k := range container.Resources.Limits {
		if strings.HasPrefix(string(k), xpuType) {
			device := strings.TrimPrefix(string(k), xpuType)
			return device
		}
	}
	return ""
}

// ConvertMatrix2Map convert matrix to map
func ConvertMatrix2Map(matrix []string, elementList []string) (map[string]map[string]int, error) {
	matrixMap := make(map[string]map[string]int)
	for i, row := range matrix {
		elements := strings.Split(row, Comma)
		if len(elementList) != len(elements) {
			return nil, errors.New(
				fmt.Sprintf("matrix row %d length is different from element list", i))

		}
		elementi := elements[i]
		if _, ok := matrixMap[elementi]; !ok {
			matrixMap[elementi] = make(map[string]int)
		}
		for j := range elements {
			bandwidth, err := strconv.Atoi(elements[j])
			if err != nil {
				return nil, errors.New(
					fmt.Sprintf("element is not number, err: %v", err))
			}
			elementj := elements[j]
			matrixMap[elementi][elementj] = bandwidth
		}
	}
	return matrixMap, nil
}
