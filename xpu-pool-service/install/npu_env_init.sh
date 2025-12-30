#!/bin/bash
# Copyright (C) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.

set -e

host_name=$(hostname)
kubectl label node ${host_name} huawei.com/vnpu=ready