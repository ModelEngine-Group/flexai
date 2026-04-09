package limiter

import (
	"errors"
	"fmt"
	"net"
	"strings"
	"sync"
	"time"

	"huawei.com/xpu-exporter/common/cache"
)

const (
	maxConnection   = 1024
	maxIPConnection = 512
)

// LimitListener returns a Listener that accepts at most n connections at the same time
func LimitListener(l net.Listener, totalConnLimit, IPConnLimit, cacheSize int) (net.Listener, error) {
	if totalConnLimit < 0 || totalConnLimit > maxConnection {
		return nil, errors.New("the parameter totalConnLimit is illegal")
	}
	if IPConnLimit < 0 || IPConnLimit > maxIPConnection {
		return nil, errors.New("the parameter IPConnLimit is illegal")
	}

	bucket := make(chan struct{}, totalConnLimit)
	ll := &localLimitListener{
		Listener:    l,
		buckets:     bucket,
		ipConnLimit: int64(IPConnLimit),
	}
	if cacheSize > 0 {
		ll.ipCache = cache.New(cacheSize)
	}
	return ll, nil
}

type localLimitListener struct {
	net.Listener
	buckets     chan struct{}
	closeOnce   sync.Once
	ipCache     *cache.ConcurrencyLRUCache
	ipConnLimit int64
}

// acquire acquires the limiting semaphore. Returns true if successfully acquired, false if the listener is closed or reach the max limit
func (l *localLimitListener) acquire() bool {
	select {
	case l.buckets <- struct{}{}:
		return true
	default:
		return false
	}
}

func (l *localLimitListener) release() { <-l.buckets }

// Accept implement net.Listener interface
func (l *localLimitListener) Accept() (net.Conn, error) {
	c, err := l.Listener.Accept()
	if err != nil {
		return nil, err
	}

	// ip connection limit
	ip, cacheKey := getIpAndKey(c)
	if ip != "" && l.ipCache != nil {
		if counts, err := l.ipCache.IncreaseOne(cacheKey, -1); err == nil && counts > l.ipConnLimit {
			return closeImmediately(c, l.ipCache), nil
		}
	}

	// total top connection limit
	if l.acquire() {
		return &limitListenerConn{Conn: c, release: l.release, ipCache: l.ipCache}, nil
	}
	return closeImmediately(c, l.ipCache), nil
}

func getIpAndKey(c net.Conn) (string, string) {
	ipWithPort := c.RemoteAddr().String()
	if ipWithPort != "" {
		s := strings.Split(ipWithPort, ":")
		return s[0], fmt.Sprintf("key-conn-%s", s[0])
	}
	return "", ""
}

func closeImmediately(c net.Conn, lruCache *cache.ConcurrencyLRUCache) net.Conn {
	// once the connection reach the max limit, force close the connection
	return &limitListenerConn{Conn: c, release: func() {}, ipCache: lruCache}
}

// Close implement net.Listener interface
func (l *localLimitListener) Close() error {
	err := l.Listener.Close()
	l.closeOnce.Do(func() { close(l.buckets) })
	return err
}

type limitListenerConn struct {
	net.Conn
	releaseOnce sync.Once
	release     func()
	ipCache     *cache.ConcurrencyLRUCache
}

// Close override net.Conn interface
func (l *limitListenerConn) Close() error {
	err := l.Conn.Close()
	l.releaseOnce.Do(l.release)
	ip, cacheKey := getIpAndKey(l.Conn)
	if ip != "" && l.ipCache != nil {
		_, _ = l.ipCache.DecreaseOne(cacheKey, time.Hour)
	}
	return err
}
