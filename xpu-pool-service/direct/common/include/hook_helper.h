/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#ifndef HOOK_HELPER_H
#define HOOK_HELPER_H

#include <dlfcn.h>

#define FUNC_HOOK_BEGIN(function, ...) __attribute__((visibility("default"))) function(__VA_ARGS__) { \
    tracepoint \
    static decltype(function) * const original = \
        reinterpret_cast<decltype(function) *>(dlsym(RTLD_NEXT, #function)); \
        { \

#define FUNC_HOOK_END }}

#define PROC_ADDR_PAIR(function) {dlsym(RTLD_NEXT, #function), reinterpret_cast<void *>(&function)}

#endif
