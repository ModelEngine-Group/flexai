/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

package allocator

import (
	"volcano.sh/volcano/pkg/scheduler/plugins/xpu-scheduler-plugin/util"
)

type nodeAssignment [][]int

type nodeAllocation struct {
	nodeResource NodeResource
	podRequests  []PodCardRequest
}

func Allocate(nodes []NodeResource, podRequests []PodCardRequest, reqXPUInterBandwidth map[string]map[string]int) ([]PodAllocation, error) {
	var allocations []PodAllocation

	minInvalidPodCount := len(podRequests) + 1
	combinations := allCombinationOfNodeAllocation(nodes, podRequests)

	for _, combination := range combinations {
		valid, allocation := validNodeAllocation(combination, nodes, podRequests)
		if !valid {
			continue
		}
		if len(reqXPUInterBandwidth) != 0 && (len(util.XPUTopologyNodeBandwidth) == 0 || !verifyInterBandwidth(allocation, reqXPUInterBandwidth)) {
			continue
		}

		if ok, podAllocations, count := tryMakePodAllocation(allocation); ok {
			if count == 0 {
				return podAllocations, nil
			}
			if count < minInvalidPodCount {
				minInvalidPodCount = count
				allocations = podAllocations
			}
		}
	}
	if len(allocations) != 0 {
		return allocations, nil
	}
	return nil, ErrCannotAllocation
}

func tryMakePodAllocation(allocations []nodeAllocation) (bool, []PodAllocation, int) {
	result := make([]PodAllocation, 0)
	invalidPodTotalCount := 0
	for _, allocation := range allocations {
		podAllocation, invalidPodCount := performPodAllocation(allocation.nodeResource, allocation.podRequests)
		if len(podAllocation) == 0 {
			return false, nil, 0
		}
		invalidPodTotalCount += invalidPodCount
		result = append(result, podAllocation...)
	}
	return true, result, invalidPodTotalCount
}

func allCombinationOfNodeAllocation(nodes []NodeResource, pods []PodCardRequest) []nodeAssignment {
	result := make([]nodeAssignment, 0, len(nodes))
	generateAllCombinations(len(nodes), len(pods), 0, make(nodeAssignment, len(nodes)), &result)
	return result
}

func generateAllCombinations(nodeCnt int, podCnt int, idx int, contents nodeAssignment, result *[]nodeAssignment) {
	if idx == podCnt {
		*result = append(*result, cloneNodeAssignment(contents))
		return
	}

	for i := 0; i < nodeCnt; i++ {
		if i >= len(contents) {
			continue
		}
		contents[i] = append(contents[i], idx)
		generateAllCombinations(nodeCnt, podCnt, idx+1, contents, result)
		contents[i] = contents[i][:len(contents[i])-1]
	}
}

func cloneNodeAssignment(contents nodeAssignment) nodeAssignment {
	duplicate := make(nodeAssignment, len(contents))
	for i := range contents {
		duplicate[i] = make([]int, len(contents[i]))
		copy(duplicate[i], contents[i])
	}
	return duplicate
}

func validNodeAllocation(assignments nodeAssignment, nodes []NodeResource, podRequests []PodCardRequest) (bool, []nodeAllocation) {
	result := make([]nodeAllocation, 0, len(assignments))
	for nodeIdx, podIndexes := range assignments {
		if len(podIndexes) == 0 {
			continue
		}

		requests := make([]PodCardRequest, 0, len(podIndexes))
		deviceRequestOnNode := 0
		for _, podIdx := range podIndexes {
			if podIdx < len(podRequests) {
				deviceRequestOnNode += podRequests[podIdx].NumberOfCard
				requests = append(requests, podRequests[podIdx])
			}
		}
		if nodeIdx >= len(nodes) {
			return false, nil
		}
		if deviceRequestOnNode > len(nodes[nodeIdx].UnuseDevices) {
			return false, nil
		}
		result = append(result, nodeAllocation{
			nodeResource: nodes[nodeIdx],
			podRequests:  requests,
		})
	}

	return true, result
}

func verifyInterBandwidth(allocation []nodeAllocation, reqXPUInterBandwidth map[string]map[string]int) bool {
	for i := range allocation {
		if len(allocation[i].podRequests) == 0 {
			continue
		}
		nodei := allocation[i].nodeResource.NodeName
		_, ok1 := util.XPUTopologyNodeBandwidth[nodei]
		for j := i + 1; j < len(allocation); j++ {
			nodej := allocation[j].nodeResource.NodeName
			_, ok2 := util.XPUTopologyNodeBandwidth[nodej]
			nodeBandwidth := 0
			if ok1 && ok2 {
				nodeBandwidth = util.XPUTopologyNodeBandwidth[nodei][nodej]
			}
			if !varifyPodInterBandwidth(allocation[i].podRequests, allocation[j].podRequests, nodeBandwidth, reqXPUInterBandwidth) {
				return false
			}
		}
	}
	return true
}

func varifyPodInterBandwidth(podRequests1 []PodCardRequest, podRequests2 []PodCardRequest, nodeBandwidth int, reqXPUInterBandwidth map[string]map[string]int) bool {
	for i := range podRequests1 {
		taski := podRequests1[i].TaskName
		if _, ok := reqXPUInterBandwidth[taski]; !ok {
			continue
		}
		for j := range podRequests2 {
			taskj := podRequests2[j].TaskName
			if _, ok := reqXPUInterBandwidth[taskj]; !ok {
				continue
			}
			needBandwidth := reqXPUInterBandwidth[taski][taskj]
			if needBandwidth > nodeBandwidth {
				return false
			}
		}
	}
	return true
}

func performPodAllocation(node NodeResource, podRequests []PodCardRequest) ([]PodAllocation, int) {
	var successfulAllocations []PodAllocation

	mask := initializeAllocatedMask(node.Topology, podRequests)
	masks := permuteUniqueAllocation(mask)
	minInvalidPodCount := len(podRequests) + 1

	for _, mask := range masks {
		deviceIds := buildDeviceAllocation(mask, podRequests)
		isGoodAllocation, allocations := goodPodAllocation(deviceIds, node, podRequests)

		if !isGoodAllocation {
			continue
		}

		if !numa {
			return allocations, 0
		}

		meetsNumaConstraints, invalidPodCount := checkNumaConstraints(node, allocations)

		if !meetsNumaConstraints {
			continue
		}

		if invalidPodCount == 0 {
			return allocations, 0
		}

		if invalidPodCount < minInvalidPodCount {
			minInvalidPodCount = invalidPodCount
			successfulAllocations = allocations
		}
	}
	return successfulAllocations, minInvalidPodCount
}

func initializeAllocatedMask(topology [][]int, podRequests []PodCardRequest) []int {
	var (
		i      = 0
		result = make([]int, len(topology))
	)
	for idx, req := range podRequests {
		for cnt := req.NumberOfCard; cnt > 0; cnt-- {
			result[i] = idx
			i++
		}
	}
	for i < len(topology) {
		result[i] = len(podRequests)
		i++
	}
	return result
}

func permuteUniqueAllocation(mask []int) [][]int {
	result := make([][]int, 0)
	generateUniqueAllocationPermutation(mask, 0, make([]bool, len(mask)), []int{}, &result)
	return result
}

func generateUniqueAllocationPermutation(nums []int, idx int, visited []bool, contents []int, result *[][]int) {
	if idx == len(nums) {
		*result = append(*result, append([]int(nil), contents...))
		return
	}

	for i := 0; i < len(nums); i++ {
		if i >= len(visited) || visited[i] {
			continue
		}
		if i > 0 && nums[i] == nums[i-1] && !visited[i-1] {
			continue
		}

		visited[i] = true
		generateUniqueAllocationPermutation(nums, idx+1, visited, append(contents, nums[i]), result)
		visited[i] = false
	}
}

func buildDeviceAllocation(mask []int, podRequests []PodCardRequest) [][]int {
	deviceIds := make([][]int, len(podRequests))
	for xpuId, podId := range mask {
		if podId >= len(podRequests) {
			continue
		}
		deviceIds[podId] = append(deviceIds[podId], xpuId)
	}
	return deviceIds
}

func goodPodAllocation(deviceIds [][]int, node NodeResource, podRequests []PodCardRequest) (bool, []PodAllocation) {
	allocates := make([]PodAllocation, len(podRequests))
	for i := range deviceIds {
		if i >= len(podRequests) {
			return false, nil
		}
		if !checkTopology(node.Topology, deviceIds[i], podRequests[i]) {
			return false, nil
		}
		for _, id := range deviceIds[i] {
			if device, ok := node.UnuseDevices[id]; !ok ||
				(len(podRequests[i].CardType) != 0 && device.Type != podRequests[i].CardType) {
				return false, nil
			}
		}

		allocates[i] = PodAllocation{
			DeviceIds: deviceIds[i],
			NodeName:  node.NodeName,
			TaskId:    podRequests[i].TaskId,
		}
	}
	return true, allocates
}

func checkTopology(topology [][]int, xpuIds []int, podRequest PodCardRequest) bool {
	for i := 0; i < len(xpuIds)-1; i++ {
		for j := i + 1; j < len(xpuIds); j++ {
			var (
				row int = 0
				col int = 0
			)
			if i < len(xpuIds) {
				row = xpuIds[i]
			}
			if j < len(xpuIds) {
				col = xpuIds[j]
			}
			if row >= len(topology) || col >= len(topology[row]) {
				return false
			}
			if topology[row][col] < podRequest.IntraBandWidth {
				return false
			}
		}
	}
	return true
}

func SetNumaConfig(enable bool) {
	numa = enable
}

func checkNumaConstraints(nodeResource NodeResource, allocations []PodAllocation) (bool, int) {
	const noNumaNode = -1
	var invalidPodCount = 0

	for _, alloc := range allocations {
		preNumaNode := noNumaNode
		for _, id := range alloc.DeviceIds {
			xpuDevice, exists := nodeResource.UnuseDevices[id]
			if !exists {
				return false, 0
			}
			if preNumaNode != noNumaNode && xpuDevice.Numa != preNumaNode {
				invalidPodCount++
				break
			}
			preNumaNode = xpuDevice.Numa
		}
	}
	return true, invalidPodCount
}
