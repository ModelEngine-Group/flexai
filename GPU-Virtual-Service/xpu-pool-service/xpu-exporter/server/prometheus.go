/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

// Package server implement the HTTP service for Prometheus to obtain monitoring data
package server

import (
	"context"
	"crypto/tls"
	"errors"
	"fmt"
	"net"
	"net/http"
	"os"
	"path/filepath"
	"regexp"
	"strconv"
	"strings"
	"time"

	"github.com/prometheus/client_golang/prometheus"
	"github.com/prometheus/client_golang/prometheus/promhttp"

	"huawei.com/xpu-exporter/collector"
	"huawei.com/xpu-exporter/common/limiter"
)

type ProtocolType int

const (
	// HTTP represents the protocol type for unencrypted HTTP communication
	HTTP ProtocolType = iota
	// HTTPS represents the protocol type for encrypted HTTPS communication
	HTTPS
	portMin                = 1025
	portMax                = 40000
	timeout                = 10
	maxHeaderBytes         = 3072
	maxIPConnLimit         = 128
	maxConcurrency         = 512
	defaultShutDownTimeout = 30 * time.Second
	certFilePath           = "/opt/xpu/certs/"
)

// ExporterServer provides the HTTP/HTTPS service for Prometheus to obtain monitoring data.
type ExporterServer struct {
	// Ip identifies the address that the server is listening on
	Ip string
	// Port for the http service
	Port int
	// Concurrency identifies the maximum concurrency of the http service
	Concurrency int
	// LimitIPReq identifies the maximum number of requests allowed per minute per IP
	LimitIPReq string
	// LimitIPConn indicates the maximum number of connections that can be created for each IP address.
	LimitIPConn int
	// LimitTotalConn identifies the maximum number of connections that the service can handle
	LimitTotalConn int
	// ProtocolType identifies which protocol to use (HTTP or HTTPS)
	ProtocolType ProtocolType
	// Collector service instance
	collectService collector.ICollectorService
	// cert stores the pre-loaded TLS certificate for HTTPS server to avoid repeated file I/O during handshakes.
	cert *tls.Certificate
}

func indexHandler(s *ExporterServer) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		s.indexHandler(w, r)
	}
}

func (s *ExporterServer) indexHandler(w http.ResponseWriter, _ *http.Request) {
	var protocol = "http"
	if s.ProtocolType == HTTPS {
		protocol = "https"
	}
	_, _ = w.Write([]byte(
		`<html>
		<head><title>XPU-Exporter</title></head>
		<body>
		<h1 align="center">XPU-Exporter</h1>
		<p align="center">Welcome to use XPU-Exporter, the Prometheus metrics url is ` + protocol + `://IP:` + strconv.Itoa(s.Port) + `/metrics: <a href="./metrics">Metrics</a></p>
		</body>
		</html>`))
}

// VerifyServerParams verify server params valid
func (s *ExporterServer) VerifyServerParams() error {
	if s.Port < portMin || s.Port > portMax {
		return errors.New("the Port is invalid")
	}
	parsedIP := net.ParseIP(s.Ip)
	if parsedIP == nil {
		return errors.New("the listen Ip is invalid")
	}
	s.Ip = parsedIP.String()

	reg := regexp.MustCompile(limiter.IPReqLimitReg)
	if !reg.Match([]byte(s.LimitIPReq)) {
		return errors.New("limitIPReq format error")
	}

	if s.LimitIPConn < 1 || s.LimitIPConn > maxIPConnLimit {
		return errors.New("limitIPConn is invalid")
	}
	if s.LimitTotalConn < 1 || s.LimitTotalConn > maxConcurrency {
		return errors.New("limitTotalConn is invalid")
	}
	if s.Concurrency < 1 || s.Concurrency > maxConcurrency {
		return errors.New("concurrency is invalid")
	}

	// Check HTTPS configuration if needed
	if os.Getenv("HTTPS_ENABLE") == "on" {
		s.ProtocolType = HTTPS
		certPath, err := getCertsFile(".crt")
		if err != nil {
			return fmt.Errorf("HTTPS certificate error: %v", err)
		}
		keyPath, err := getCertsFile(".key")
		if err != nil {
			return fmt.Errorf("HTTPS key error: %v", err)
		}
		cert, err := tls.LoadX509KeyPair(certPath, keyPath)
		if err != nil {
			return fmt.Errorf("HTTPS key pair error: %v", err)
		}
		s.cert = &cert
	} else {
		s.ProtocolType = HTTP
	}
	return nil
}

func getTLSConfig() *tls.Config {
	return &tls.Config{
		MinVersion:               tls.VersionTLS13,
		PreferServerCipherSuites: true,
		InsecureSkipVerify:       false,
	}
}

func getCertsFile(suffix string) (string, error) {
	files, err := os.ReadDir(certFilePath)
	if err != nil {
		return "", err
	}
	for _, file := range files {
		if file.IsDir() {
			continue
		}
		if strings.HasSuffix(file.Name(), suffix) {
			return filepath.Join(certFilePath, file.Name()), nil
		}
	}
	return "", fmt.Errorf("no files with the suffix '%s' found in directory: %s", suffix, certFilePath)
}

func (s *ExporterServer) initConfig() *limiter.HandlerConfig {
	conf := &limiter.HandlerConfig{
		PrintLog:         true,
		Method:           http.MethodGet,
		LimitBytes:       limiter.DefaultDataLimit,
		TotalConCurrency: s.Concurrency,
		IPConCurrency:    s.LimitIPReq,
		CacheSize:        limiter.DefaultCacheSize,
	}
	return conf
}

func (s *ExporterServer) newServer(conf *limiter.HandlerConfig) (*http.Server, net.Listener, error) {
	handler, err := limiter.NewLimitHandler(http.DefaultServeMux, conf)
	if err != nil {
		return nil, nil, err
	}

	server := &http.Server{
		Addr:           s.Ip + ":" + strconv.Itoa(s.Port),
		Handler:        handler,
		ReadTimeout:    timeout * time.Second,
		WriteTimeout:   timeout * time.Second,
		MaxHeaderBytes: maxHeaderBytes,
	}

	// Configure TLS if needed
	if s.ProtocolType == HTTPS {
		server.TLSConfig = getTLSConfig()
		server.TLSConfig.GetCertificate = func(*tls.ClientHelloInfo) (*tls.Certificate, error) {
			if s.cert == nil {
				return nil, fmt.Errorf("certificate not loaded")
			}
			return s.cert, nil
		}
	}

	l, err := net.Listen("tcp", server.Addr)
	if err != nil {
		return nil, nil, fmt.Errorf("listen IP and port error: %v", err)
	}

	limitLs, err := limiter.LimitListener(l, s.LimitTotalConn, s.LimitIPConn, limiter.DefaultCacheSize)
	if err != nil {
		return nil, nil, fmt.Errorf("limit listener error: %v", err)
	}

	return server, limitLs, nil
}

// StartServe starts the HTTP or HTTPS server based on configuration
func (s *ExporterServer) StartServe(ctx context.Context, cancel context.CancelFunc, reg *prometheus.Registry) {
	http.Handle("/metrics", promhttp.HandlerFor(reg, promhttp.HandlerOpts{ErrorHandling: promhttp.ContinueOnError}))
	http.Handle("/", indexHandler(s))

	conf := s.initConfig()
	server, listener, err := s.newServer(conf)
	if err != nil {
		cancel()
		return
	}

	go func() {
		var err error
		if s.ProtocolType == HTTPS {
			err = server.ServeTLS(listener, "", "")
		} else {
			err = server.Serve(listener)
		}

		if err != nil && err != http.ErrServerClosed {
			cancel()
		}
	}()

	<-ctx.Done()

	shutdownCtx, cancelShutdown := context.WithTimeout(context.Background(), defaultShutDownTimeout)
	defer cancelShutdown()
	if err := server.Shutdown(shutdownCtx); err != nil {
	}
}

// RegisterCollectorService register the collectorService based on the XPU type.
func (s *ExporterServer) RegisterCollectorService(c collector.ICollectorService) error {
	if s == nil {
		return fmt.Errorf("s is nil")
	}
	if c == nil {
		return fmt.Errorf("collector service is nil")
	}
	s.collectService = c
	return nil
}

// CreateCollector create a matching collector.
func (s *ExporterServer) CreateCollector(cacheTime time.Duration, updateTime time.Duration) prometheus.Collector {
	return s.collectService.CreateCollector(cacheTime, updateTime)
}

// StartCollect starting periodic XPU information collection
func (s *ExporterServer) StartCollect(ctx context.Context, fn context.CancelFunc) {
	s.collectService.Start(ctx, fn)
}
