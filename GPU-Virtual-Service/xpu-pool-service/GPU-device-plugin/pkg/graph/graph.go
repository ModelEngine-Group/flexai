/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

package graph

import (
	"strconv"
	"strings"
)

// TopologyProvider is the interface that provides topology information for device.
type TopologyProvider interface {
	// Topology() returns xpu topology information of the node.
	Topology() string
}

// TopologyGraph represents XPU topology using adjacency matrix, which means a two dimensional array.
type TopologyGraph [][]int

// NewTopologyGraph new a topology graph with capability.
func NewTopologyGraph(capability int) TopologyGraph {
	if capability < 0 {
		panic("capability must not be negative")
	}
	graph := make(TopologyGraph, capability)
	for i := range graph {
		graph[i] = make([]int, capability)
	}
	return graph
}

// GetTopologyGraph returns the string representation of the topology.
func (graph TopologyGraph) GetTopologyGraph() string {
	return graph.Serializer()
}

// Serialize serializes the graph into a string.
// Each items of row is separated by comma (,), then
// each row is separated by semicolon (;).
// For example, a topology graph is
// [
//
//	[0, 10],
//	[10, 0]
//
// ]
// the result of serialize is "0,10;10,0".
func (graph TopologyGraph) Serializer() string {
	result := make([]string, 0, len(graph))
	for _, row := range graph {
		tokens := make([]string, 0, len(row))
		for _, v := range row {
			tokens = append(tokens, strconv.Itoa(v))
		}
		result = append(result, strings.Join(tokens, ","))
	}
	return strings.Join(result, ";")
}
