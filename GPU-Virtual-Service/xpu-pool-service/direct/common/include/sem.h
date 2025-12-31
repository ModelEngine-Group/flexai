/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#ifndef SEM_H
#define SEM_H

#include <condition_variable>
#include <mutex>

class Sem {
public:
    explicit Sem(int count = 0) : count_(count)
    {}

    void Release(int count = 1)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        count_ += count;
        cv_.notify_all();
    }

    void Aquire(int count = 1)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [=] { return count_ >= count; });
        count_ -= count;
    }

    int AquireAll()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        int count = count_;
        count_ = 0;
        return count;
    }

    template <class Rep, class Period>
    bool TryAquireFor(int count = 1, const std::chrono::duration<Rep, Period> &waitMax = 0)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (waitMax == waitMax.zero() && count_ < count) {
            return false;
        }
        return cv_.wait_for(lock, waitMax, [=] { return count_ >= count; });
    }

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    int count_;
};

#endif