/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#ifndef COMMON_H
#define COMMON_H

const int RET_SUCC = 0;
const int RET_FAIL = 1;

constexpr unsigned long MICROSEC = 1000UL * 1000UL;
constexpr unsigned long MEGABYTE = 1024UL * 1024UL;
constexpr unsigned int PERCENT_MIN = 0U;
constexpr unsigned int PERCENT_MAX = 100U;

#ifdef UNIT_TEST
#define TESTABLE_PROTECTED public
#define TESTABLE_PRIVATE public
#else
#define TESTABLE_PRIVATE private
#define TESTABLE_PROTECTED protected
#endif

#endif