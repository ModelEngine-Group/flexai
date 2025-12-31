#!/bin/bash
# Copyright (C) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
set -e

function arch_config() {
    arch=$(uname -m)
    if [[ ${arch} == "x86_64" ]]; then
        platform="x86"
    elif [[ ${arch} == "aarch64" ]]; then
        platform="arm"
    else
        echo "incorrect arch mode"
        exit 1
    fi
}

build_version=$(cat buildInfo.properties | sed -n 's/.*=//p')
echo ${build_version}

# get product from CI pipeline for this project:
artget pull 56e9abca9a9045a98c283fd0cc958ffc ${build_version} -ca snapshot -at cloudartifact -ap deploy

ssh -o "StrictHostKeyChecking no" ${execute_environment} "rm -rf /data/ci/at"
cd ${WORKSPACE}/${branch}/test
scp -r at ${execute_environment}:/data/ci/

arch_config
upload_arch=$(echo ${arch} | sed 's/_/-/g')
cd ${WORKSPACE}/deploy/software
scp ${upload_version}_${upload_arch}.zip ${execute_environment}:/data/ci/at/
ssh ${execute_environment} "cd /data/ci/at && sh runtest.sh --artifact ${upload_version}_${upload_arch}.zip"