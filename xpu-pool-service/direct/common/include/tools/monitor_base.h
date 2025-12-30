/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#ifndef MONITOR_BASE_H
#define MONITOR_BASE_H

#include <cctype>
#include <future>
#include <map>
#include "resource_config.h"
#include "log.h"

namespace xpu {

enum class OutputFormat : char {
    NONE = '\0',
    TABLE = 't',
    JSON = 'j',
};

enum class VxpuType : char {
    NONE = '\0',
    VGPU = 'G',
    VNPU = 'N',
};

constexpr int PERIOD_DEFAULT = 60;   // one minute
constexpr int PERIOD_MIN = 1;        // one second
constexpr int PERIOD_MAX = 60 * 60 * 24; // one day
constexpr int MAX_PIDS = 1024;

struct Args {
    int period = PERIOD_DEFAULT;
    OutputFormat format = OutputFormat::TABLE;
};

int ParseArgs(Args &args, int argc, char *const argv[]);

struct VxpuFormatter {
    OutputFormat format_ = OutputFormat::NONE;

    constexpr auto parse(fmt::format_parse_context &ctx)
    {
        for (auto it = ctx.begin(); it != ctx.end(); it++) {
            if (*it == 't' || *it == 'j') {
                format_ = OutputFormat(*it);
            } else if (*it == '}') {
                return it;
            } else {
                ctx.on_error("invalid output format");
                return it;
            }
        }
        return ctx.end();
    }
};

struct ProcessInfo {
    uint32_t core = 0;
    size_t memory = 0;
};

struct VxpuInfo {
    VxpuType type;
    uint32_t id;
    uint32_t coreQuota;
    uint32_t core = 0;
    size_t memory = 0;
    size_t memoryQuota;
    std::map<uint32_t, ProcessInfo> processes;

    VxpuInfo(ResourceConfig &config, VxpuType type, int32_t id) : type(type), id(id)
    {
        if (config.LimitComputingPower()) {
            coreQuota = config.ComputingPowerQuota();
        } else {
            coreQuota = PERCENT_MAX;
        }
        if (config.LimitMemory()) {
            memoryQuota = config.MemoryQuota();
        } else {
            memoryQuota = 0;
        }
    }

TESTABLE_PRIVATE:
    VxpuInfo(VxpuType type): type(type), coreQuota(PERCENT_MAX), memoryQuota(0) {}
};

struct ContainerVxpuInfo {
    VxpuType type;
    std::vector<VxpuInfo> vxpus;

    ContainerVxpuInfo(VxpuType type) : type(type)
    {}
};

}// namespace xpu
template <>
class fmt::formatter<std::pair<const uint32_t, xpu::ProcessInfo>> : public xpu::VxpuFormatter {
public:
    template <typename Context>
    auto format(const std::pair<const uint32_t, xpu::ProcessInfo> &info, Context &ctx) const
    {
        if (format_ == xpu::OutputFormat::JSON) {
            return format_to(ctx.out(),
                "{{\"pid\": {}, \"core\": {}, \"memory\": {}}}",
                info.first,
                info.second.core,
                info.second.memory);
        } else {
            return format_to(ctx.out(),
                "pid {}, core usage {:02}%, memory usage {:6}MB",
                info.first,
                info.second.core,
                info.second.memory / MEGABYTE);
        }
    }
};

template <>
class fmt::formatter<xpu::VxpuInfo> : public xpu::VxpuFormatter {
public:
    template <typename Context>
    auto format(const xpu::VxpuInfo &info, Context &ctx) const
    {
        if (format_ == xpu::OutputFormat::JSON) {
            return format_to(ctx.out(),
                "{{\"device\": {}, \"core\": {}, \"core_quota\": {}, \"memory\": {}, \"memory_quota\": {},\n"
                "\"processes\": [{:j}]}}",
                info.id,
                info.core,
                info.coreQuota,
                info.memory,
                info.memoryQuota,
                fmt::join(info.processes, ",\n"));
        } else {
            return format_to(ctx.out(),
                "v{}PU {} usage {:02}%, limit {:02}%, memory usage {:6}/{}MB\n{:t}",
                char(info.type),
                info.id,
                info.core,
                info.coreQuota,
                info.memory / MEGABYTE,
                info.memoryQuota / MEGABYTE,
                fmt::join(info.processes, "\n"));
        }
    }
};

template <>
class fmt::formatter<xpu::ContainerVxpuInfo> : public xpu::VxpuFormatter {
public:
    template <typename Context>
    auto format(const xpu::ContainerVxpuInfo &info, Context &ctx) const
    {
        if (format_ == xpu::OutputFormat::JSON) {
            return format_to(ctx.out(),
                "{{\"type\": \"v{}PU\", \"vxpus\": [\n{:j}\n]}}",
                char(info.type),
                fmt::join(info.vxpus, ",\n"));
        } else {
            return format_to(
                ctx.out(), "v{}PU num: {}\n{:t}", char(info.type), info.vxpus.size(), fmt::join(info.vxpus, "\n"));
        }
    }
};

#endif
