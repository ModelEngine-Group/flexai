/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#ifndef NPU_TIMESLICE_SCHEDULER_H
#define NPU_TIMESLICE_SCHEDULER_H

#include <atomic>
#include <chrono>
#include <cstdint>
#include "common.h"

/**
* 并行访问约定:
* 1. 每个node的node编号(进程)持有一个node编号, 每个容器只有一个进程
*    进程内的访问和更新由进程自行保障.
* 2. 所有聚集和调度相关数据是Node结构体.
* 3. 访问者只能读取除current字段外的所有字段, 访问者视为活跃.
* 4. 如果访问者持有编号等于current字段, 访问者视为活跃.
*    可以访问所有公共字段(Node额外的所有字段).
* 5. 加入退出是即时的.
*    访问者退出时, 即使自己Node的时间戳是脏的.
* 6. 活跃时间更新后写一个时间戳更新的节点.
*    活跃时间是当前节点的时间.
* 7. 当node->currentNode的时间戳比当前时间大时,
*    我们认为该node对应的调度者已经终止.
* 8. 当访问者看到当前活跃时间已经终止时,
*    可以尝试CAS current以进入中兴
*/
class NpuTimesliceScheduler {
public:
    using clock = std::chrono::steady_clock;

    NpuTimesliceScheduler();
    ~NpuTimesliceScheduler();
    int Init(int idx, void *context, unsigned int quota);
    clock::time_point UpdateTimestamp();
    bool CheckCurrent();
    void ReleaseCurrent();
    void SchedulerThread(bool &terminating);
    bool IsValid();
    clock::duration TimeUnit();


TESTABLE_PRIVATE:
    using AtomicTimestamp = std::atomic<clock::time_point>;
    constexpr static int MAGIC_NUMBER_INIT = ('i' << 24) | ('n' << 16) | ('i' << 8) | 't';
    constexpr static int MAGIC_NUMBER = ('v' << 24) | ('N' << 16) | ('P' << 8) | 'U';
    constexpr static int PERIOD_UNIT_NUMBER = 100;
    constexpr static int MIN_COMPUTE_POWER = 5;
    constexpr static int MAX_NODE_NUMBER = PERIOD_UNIT_NUMBER / MIN_COMPUTE_POWER;
    // 调度轮询时间单位
    constexpr static clock::duration TIME_UNIT = std::chrono::milliseconds(1);
    // 调度轮询周期，用于时间片轮转时过滤未占用节点
    constexpr static auto PERIOD_TIMEOUT = TIME_UNIT * PERIOD_UNIT_NUMBER;
    // 占用超时时间，用于从死亡等待中恢复（初始化失败或current节点崩溃）
    constexpr static auto ERR_CHECK_TIMEOUT = std::chrono::seconds(1);

    struct Context {
        struct Node {
            AtomicTimestamp periodCheck;
        };
        std::atomic<uint32_t> magicNumber;
        clock::duration timeUnit;
        unsigned int usedUnits;
        std::atomic<int> current;
        Node nodes[MAX_NODE_NUMBER];
    };

    int idx_;
    Context *context_;
    
    clock::duration currentSlice_;
    clock::duration quota_;
    unsigned int quotaPercent_;
    unsigned int lastUsedUnits_ = 0;
    bool lastUsedUnitsValid_ = false;
    
    void SelectNewCurrent();
    clock::time_point ExecuteTimeSlice(clock::time_point begin);
    void ExecuteIdleTime();

public:
    constexpr static size_t CONTEXT_SIZE = sizeof(Context);
};

#endif