/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

// Package lock implements the node lock, and provide k8s cluster client access entry
package lock

import (
	"context"
	"fmt"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"time"

	"huawei.com/vxpu-device-plugin/pkg/log"
	"k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/rest"
	"k8s.io/client-go/tools/clientcmd"
)

const (
	maxLockRetry        = 5
	lockRetryInterval   = 100
	lockExpiredInterval = 300.0          // 设定操作容忍范围，容器内和主机的两个/proc/uptime文件实际上是指向同一个宿主主机维护的文件
	uptimeFilePath      = "/proc/uptime" // 避免对设备插件重启后可能带来的时间差问题
)

func getUptime() (string, error) {
	uptimeFileBytes, err := os.ReadFile(uptimeFilePath)
	if err != nil {
		return "", err
	}
	eles := strings.Fields(string(uptimeFileBytes))
	return eles[0], nil
}

var kubeClient kubernetes.Interface

// GetClient return a k8s client connection to apiserver
func GetClient() kubernetes.Interface {
	return kubeClient
}

// NewClient create a k8s client connection to apiserver
func NewClient() error {
	kubeConfig := os.Getenv("KUBECONFIG")
	if kubeConfig == "" {
		kubeConfig = filepath.Join(os.Getenv("HOME"), ".kube", "config")
	}
	config, err := rest.InClusterConfig()
	if err != nil {
		config, err = clientcmd.BuildConfigFromFlags("", kubeConfig)
		if err != nil {
			return err
		}
	}
	client, err := kubernetes.NewForConfig(config)
	kubeClient = client
	return err
}

func setNodeLock(nodeName string, lockName string) error {
	ctx := context.Background()
	node, err := kubeClient.CoreV1().Nodes().Get(ctx, nodeName, v1.GetOptions{})
	if err != nil {
		return err
	}
	if lockName == "" {
		return fmt.Errorf("lockName is empty")
	}
	if _, ok := node.ObjectMeta.Annotations[lockName]; ok {
		return fmt.Errorf("node %s is locked", nodeName)
	}
	newNode := node.DeepCopy()
	uptime, err := getUptime()
	if err != nil {
		return err
	}
	newNode.ObjectMeta.Annotations[lockName] = uptime
	_, err = kubeClient.CoreV1().Nodes().Update(ctx, newNode, v1.UpdateOptions{})
	for i := 1; i <= maxLockRetry && err != nil; i++ {
		time.Sleep(lockRetryInterval * time.Millisecond)
		node, err = kubeClient.CoreV1().Nodes().Get(ctx, nodeName, v1.GetOptions{})
		if err != nil {
			log.Errorf("Failed to get node when retry to update: %v, node: %s", err, nodeName)
			continue
		}
		newNode = node.DeepCopy()
		uptime, err = getUptime()
		if err != nil {
			continue
		}
		newNode.ObjectMeta.Annotations[lockName] = uptime
		_, err = kubeClient.CoreV1().Nodes().Update(ctx, newNode, v1.UpdateOptions{})
	}
	if err != nil {
		return fmt.Errorf("setNodeLock exceeds retry count %d", maxLockRetry)
	}
	log.Infof("Node lock set, node: %s", nodeName)
	return nil
}

// ReleaseNodeLock releases a certain lock on a node
func ReleaseNodeLock(nodeName string, lockName string) error {
	ctx := context.Background()
	node, err := kubeClient.CoreV1().Nodes().Get(ctx, nodeName, v1.GetOptions{})
	if err != nil {
		return err
	}
	if _, ok := node.ObjectMeta.Annotations[lockName]; !ok {
		return nil
	}
	newNode := node.DeepCopy()
	delete(newNode.ObjectMeta.Annotations, lockName)
	_, err = kubeClient.CoreV1().Nodes().Update(ctx, newNode, v1.UpdateOptions{})
	for i := 1; i <= maxLockRetry && err != nil; i++ {
		time.Sleep(lockRetryInterval * time.Millisecond)
		node, err = kubeClient.CoreV1().Nodes().Get(ctx, nodeName, v1.GetOptions{})
		if err != nil {
			log.Errorf("Failed to get node when retry to update: %v, node: %s", err, nodeName)
			continue
		}
		newNode = node.DeepCopy()
		delete(newNode.ObjectMeta.Annotations, lockName)
		_, err = kubeClient.CoreV1().Nodes().Update(ctx, newNode, v1.UpdateOptions{})
	}
	if err != nil {
		return fmt.Errorf("releaseNodeLock exceeds retry count %d", maxLockRetry)
	}
	log.Infof("Node lock released, node: %s", nodeName)
	return nil
}

// ObtainLockNode obtains a certain lock on a node
func ObtainLockNode(nodeName string, lockName string) error {
	ctx := context.Background()
	node, err := kubeClient.CoreV1().Nodes().Get(ctx, nodeName, v1.GetOptions{})
	if err != nil {
		return err
	}
	if _, ok := node.ObjectMeta.Annotations[lockName]; !ok {
		return setNodeLock(nodeName, lockName)
	}
	lockTime, err := strconv.ParseFloat(node.ObjectMeta.Annotations[lockName], 64)
	if err != nil {
		log.Errorf("Failed to parse float: %v, strTime: %s", err, node.ObjectMeta.Annotations[lockName])
		return err
	}
	uptime, err := getUptime()
	if err != nil {
		return err
	}
	curTime, err := strconv.ParseFloat(uptime, 64)
	if err != nil {
		log.Errorf("Failed to parse float: %v, strTime: %s", err, uptime)
		return err
	}
	if curTime-lockTime > lockExpiredInterval {
		err = ReleaseNodeLock(nodeName, lockName)
		if err != nil {
			log.Errorf("Failed to release node lock: %v, node: %s", err, nodeName)
			return err
		}
		return setNodeLock(nodeName, lockName)
	}
	return fmt.Errorf("node %s has been locked within %f seconds", nodeName, lockExpiredInterval)
}
