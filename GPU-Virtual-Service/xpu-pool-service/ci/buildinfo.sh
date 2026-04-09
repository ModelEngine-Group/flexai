#!/bin/bash
# Copyright (C) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
set -e

echo "Release is ${ENV_IS_RELEASE}"

# 判断当前构建是否为版本构建，以决定构建变量
if [ "${ENV_IS_RELEASE}" == "false" ]; then
    SERVICE_VERSION='1.0.0-SNAPSHOT'
    echo "buildVersion=${SERVICE_VERSION}.${ENV_PIPELINE_STARTTIME}">"${WORKSPACE}"/buildInfo.properties
else
    if [ "${ENV_IS_RELEASE}" == "true" ]; then
        SERVICE_VERSION=${ENV_RELEASE_VERSION}
        echo "buildVersion=${ENV_RELEASE_VERSION}">"${WORKSPACE}"/buildInfo.properties
    fi
fi