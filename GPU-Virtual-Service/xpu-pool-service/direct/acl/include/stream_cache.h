/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#ifndef STREAM_CACHE_H
#define STREAM_CACHE_H

#include <atomic>
#include <cstdint>
#include <utility>
#include <vector>
#include <runtime/rt.h>
#include "log.h"

class StreamCache {
public:
    void SetSize(size_t size)
    {   
        Clear();
        max_ = size;
        streams_.resize(size, {0, 0});
    }

    void Clear()
    {
        for (size_t i = 0; i < current_; i++) {
            rtError_t ret = rtCtxSetCurrent(streams_[i].first);
            if (ret != RT_ERROR_NONE) {
                log_err("rtCtxSetCurrent error {}", ret);
                sleep(1);
                continue;
            }
            ret = rtStreamSynchronize(streams_[i].second);
            if (ret != RT_ERROR_NONE) {
                log_err("rtStreamSynchronize error {}", ret);
                sleep(1);
                continue;
            }
        }
        current_ = 0;
    }

    /**
    * ConcurrentPush函数允许并发调用, 但是不能与其他操作混合并发调用.
    * ConcurrentPush函数运行期间不允许调用SetSize和Clear函数.
    */
    bool ConcurrentPush(rtContext_t ctx, rtStream_t stream)
    {
        size_t idx = current_++;
        if (idx >= max_) {
            return false;
        }
        streams_[idx] = {ctx, stream};
        return true;
    }

private:
    size_t max_;
    std::atomic<size_t> current_ = 0;
    std::vector<std::pair<rtContext_t, rtStream_t>> streams_;
};

#endif