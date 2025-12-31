#!/bin/bash

set -e

BASE_VERSION='1.10.2'
GOPATH=${GOPATH:-$(go env GOPATH)}
TOP_DIR=${GOPATH}/src/volcano.sh/volcano/
BASE_PATH=${GOPATH}/src/volcano.sh/volcano/pkg/scheduler/plugins/xpu-scheduler-plugin/
CMD_PATH=${GOPATH}/src/volcano.sh/volcano/cmd/
PKG_PATH=volcano.sh/volcano/pkg
DATE=$(date "+%Y-%m-%d %H:%M:%S")
GCC_PATH=/usr/bin/gcc

PLUGIN_NAME=huawei-xpu

function clean() {
  rm -f "${BASE_PATH}"/output/vc-controller-manager
  rm -f "${BASE_PATH}"/output/vc-scheduler
  rm -f "${BASE_PATH}"/output/*.so
}

function build() {
  echo "Build start"

  export GO111MODULE=on
  export PATH=$GOPATH/bin:$PATH

  cd "${TOP_DIR}"
  go mod tidy

  cd "${BASE_PATH}"/output/

  for name in controller-manager scheduler webhook-manager; do \
    CGO_CFLAGS="-fstack-protector-strong -D_FORTIFY_SOURCE=2 -O2 -fPIC -ftrapv" \
    CGO_CPPFLAGS="-fstack-protector-strong -D_FORTIFY_SOURCE=2 -O2 -fPIC -ftrapv" \
    CC=${GCC_PATH} CGO_ENABLED=1 \
    go build -mod=mod -buildmode=pie -ldflags "-s -linkmode=external -extldflags=-Wl,-z,relro,-z,now
    -X  '${PKG_PATH}/version.Built=${DATE}' -X '${PKG_PATH}/version.Version=${BASE_VERSION}'" \
    -o vc-$name "${CMD_PATH}"/$name
  done

  CGO_CFLAGS="-fstack-protector-strong -D_FORTIFY_SOURCE=2 -O2 -fPIC -ftrapv" \
  CGO_CPPFLAGS="-fstack-protector-strong -D_FORTIFY_SOURCE=2 -O2 -fPIC -ftrapv" \
  CC=${GCC_PATH} CGO_ENABLED=1 \
  go build -mod=mod -buildmode=plugin -ldflags "-s -linkmode=external -extldflags=-Wl,-z,relro,-z,now
  -X volcano.sh/volcano/pkg/scheduler/plugins/xpu-scheduler-plugin.PluginName=${PLUGIN_NAME}" \
  -o "${PLUGIN_NAME}".so "${GOPATH}"/src/volcano.sh/volcano/pkg/scheduler/plugins/xpu-scheduler-plugin/

  if [ ! -f "${BASE_PATH}/output/${PLUGIN_NAME}.so" ]
  then
    echo "Failed to find huawei-xpu.so"
    exit 1
  fi

  chmod 400 "${BASE_PATH}"/output/*.so
  chmod 500 vc-controller-manager vc-scheduler
}

function main() {
  clean
  build
}

main

echo "Build done"
