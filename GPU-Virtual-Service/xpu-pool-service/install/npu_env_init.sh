#!/bin/bash
# Copyright (c) 2025 Shanghai Jiao Tong University.
# Contributed to the ModelEngine Community.

set -e

host_name=$(hostname)
kubectl label node ${host_name} flexai.com/vnpu=ready