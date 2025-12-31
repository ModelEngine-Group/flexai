/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

package limiter

import (
	"net"
	"testing"

	"github.com/agiledragon/gomonkey/v2"
	"github.com/stretchr/testify/assert"
)

const (
	ConnLimit = 2
)

type mockLicener struct {
}

func (l *mockLicener) Accept() (net.Conn, error) {
	return &net.TCPConn{}, nil
}

func (l *mockLicener) Addr() net.Addr {
	return &net.IPAddr{
		IP:   []byte("127.0.0.1"),
		Zone: "",
	}
}

func (l *mockLicener) Close() error {
	return nil
}

func TestLimitListener(t *testing.T) {
	l, err := LimitListener(&mockLicener{}, maxConnection, maxIPConnection, DefaultDataLimit)
	assert.NotEqual(t, nil, l)
	assert.Equal(t, nil, err)

	_, err = LimitListener(&mockLicener{}, maxConnection+1, maxIPConnection, DefaultDataLimit)
	assert.NotEqual(t, nil, err)

	_, err = LimitListener(&mockLicener{}, maxConnection, maxIPConnection+1, DefaultDataLimit)
	assert.NotEqual(t, nil, err)
}

func TestAccept(t *testing.T) {
	limitlor, err := LimitListener(&mockLicener{}, ConnLimit, ConnLimit, DefaultCacheSize)
	if err != nil {
		t.Error("Limitlistener test failed")
	}
	l, ok := limitlor.(*localLimitListener)
	if !ok {
		t.Error("test failed")
	}

	patchGetIpAndKey := gomonkey.ApplyFunc(getIpAndKey, func(net.Conn) (string, string) {
		return "127.0.0.1", "key-127.0.0.1"
	})
	defer patchGetIpAndKey.Reset()

	_, err = l.Accept()
	if err != nil {
		t.Error("Accept test failed")
	}

	patchAcquire := gomonkey.ApplyPrivateMethod(l, "acquire", func(*localLimitListener) bool {
		return false
	})
	defer patchAcquire.Reset()

	con, err := l.Accept()
	assert.Equal(t, nil, err)
	conn, ok := con.(*limitListenerConn)
	if !ok {
		t.Error("Accept test failed")
	}
	if conn.release == nil {
		t.Error("Accept test failed")
	} else {
		t.Log("Accept test success")
	}
}

func TestGetIpAndKey(t *testing.T) {
	c := net.TCPConn{}
	patch := gomonkey.ApplyMethodFunc(&c, "RemoteAddr", func() net.Addr {
		return &net.IPAddr{
			IP:   []byte("127.0.0.1"),
			Zone: "",
		}
	})
	defer patch.Reset()
	ip, _ := getIpAndKey(&c)
	assert.NotEqual(t, "", ip)
}
