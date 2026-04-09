/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

// Package server implement the HTTP service for Prometheus to obtain monitoring data
package server

import (
	"context"
	"crypto/tls"
	"errors"
	"net"
	"net/http"
	"os"
	"testing"

	"github.com/agiledragon/gomonkey/v2"
	"github.com/prometheus/client_golang/prometheus"
	"github.com/stretchr/testify/assert"
	"huawei.com/vxpu-device-plugin/pkg/log"
)

type mockCollectorService struct{}

func (m *mockCollectorService) CreateCollector(_, _ time.Duration) prometheus.Collector {
	return nil
}

func (m *mockCollectorService) Start(_ context.Context, _ context.CancelFunc) {}

func (m *mockCollectorService) GetName() string {
	return "mock-collector"
}

func TestVerifyServerParams(t *testing.T) {
	tests := getTestCases()

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			if tt.setup != nil {
				patches := tt.setup()
				defer patches.Reset()
			}

			err := tt.server.VerifyServerParams()
			if tt.wantErr {
				assert.Error(t, err)
			} else {
				assert.NoError(t, err)
			}
		})
	}
}

func getTestCases() []struct {
	name   string
	server *ExporterServer
	wantErr bool
	setup  func() *gomonkey.Patches
} {
	baseServer := func() *ExporterServer {
		return &ExporterServer{
			Port:           9119,
			Ip:             "127.0.0.1",
			Concurrency:    100,
			LimitIPReq:     "20/1",
			LimitIPConn:    10,
			LimitTotalConn: 200,
		}
	}

	return []struct {
		name   string
		server *ExporterServer
		wantErr bool
		setup  func() *gomonkey.Patches
	}{
		{
			name:   "valid http config",
			server: baseServer(),
			setup:  func() *gomonkey.Patches { return setupEnv("") },
		},
		{
			name: "invalid port",
			server: func() *ExporterServer {
				s := baseServer()
				s.Port = 1024 // 示例中的无效端口，实际应根据验证逻辑调整
				return s
			}(),
			wantErr: true,
		},
		{
			name: "invalid ip",
			server: func() *ExporterServer {
				s := baseServer()
				s.Ip = "invalid.ip"
				return s
			}(),
			wantErr: true,
		},
		{
			name: "https with invalid certs",
			server: baseServer(),
			wantErr: true,
			setup:  func() *gomonkey.Patches { return setupHTTPSFailure() },
		},
	}
}

func setupEnv(envValue string) *gomonkey.Patches {
	patches := gomonkey.NewPatches()
	patches.ApplyFunc(os.Getenv, func(string) string { return envValue })
	return patches
}

func setupHTTPSFailure() *gomonkey.Patches {
	patches := setupEnv("on")
	patches.ApplyFunc(os.ReadDir, func(string) ([]os.DirEntry, error) {
		return nil, errors.New("dir error")
	})
	return patches
}

func TestRegisterCollectorService(t *testing.T) {
	s := &ExporterServer{}
	mockCollector := &mockCollectorService{}

	t.Run("valid registration", func(t *testing.T) {
		err := s.RegisterCollectorService(mockCollector)
		assert.NoError(t, err)
		assert.Equal(t, mockCollector, s.collectService)
	})

	t.Run("nil collector", func(t *testing.T) {
		err := s.RegisterCollectorService(nil)
		assert.Error(t, err)
	})
}

func TestStartServe(t *testing.T) {
	patches := gomonkey.NewPatches()
	defer patches.Reset()

	// Mock依赖
	patches.ApplyFunc(net.Listen, func(network, address string) (net.Listener, error) {
		return &mockListener{}, nil
	})

	patches.ApplyMethodFunc(&http.Server{}, "Serve", func(l net.Listener) error {
		return nil
	})

	patches.ApplyMethodFunc(&http.Server{}, "ServeTLS", func(l net.Listener, certFile, keyFile string) error {
		return nil
	})

	patches.ApplyFunc(log.Info, func(format string, args ...interface{}) {})
	patches.ApplyFunc(log.Errorf, func(format string, args ...interface{}) {})

	t.Run("start http server", func(t *testing.T) {
		http.DefaultServeMux = http.NewServeMux() // 重置路由
		ctx, cancel := context.WithCancel(context.Background())
		defer cancel()

		s := &ExporterServer{
			Port:        9119,
			Ip:          "127.0.0.1",
			ProtocolType: HTTP,
		}
		reg := prometheus.NewRegistry()
		s.StartServe(ctx, cancel, reg)
	})

	t.Run("start https server", func(t *testing.T) {
		http.DefaultServeMux = http.NewServeMux() // 重置路由
		ctx, cancel := context.WithCancel(context.Background())
		defer cancel()

		patches.ApplyFunc(getCertsFile, func(suffix string) (string, error) {
			return "test.crt", nil
		})

		patches.ApplyFunc(tls.LoadX509KeyPair, func(certFile, keyFile string) (tls.Certificate, error) {
			return tls.Certificate{}, nil
		})

		s := &ExporterServer{
			Port:        9119,
			Ip:          "127.0.0.1",
			ProtocolType: HTTPS,
		}
		reg := prometheus.NewRegistry()
		s.StartServe(ctx, cancel, reg)
	})
}

type mockListener struct{}

func (m *mockListener) Accept() (net.Conn, error) {
	return nil, nil
}

func (m *mockListener) Close() error {
	return nil
}

func (m *mockListener) Addr() net.Addr {
	return &net.TCPAddr{}
}