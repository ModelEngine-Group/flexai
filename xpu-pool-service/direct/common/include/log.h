/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#ifndef COMMON_LOG_H
#define COMMON_LOG_H

#include <stdlib.h>
#include <spdlog/spdlog.h>

#define log_trace(fmt, ...) {\
    spdlog::trace("[{}:{}] " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
}
#define log_debug(fmt, ...) {\
    spdlog::debug("[{}:{}] " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
}
#define log_info(fmt, ...) { \
    spdlog::info("[{}:{}] " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
}
#define log_warn(fmt, ...) {\
    spdlog::warn("[{}:{}] " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
}
#define log_err(fmt, ...) {\
    spdlog::error("[{}:{}] " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
}
#define log_critical(fmt, ...) {\
    spdlog::critical("[{}:{}] " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
}
#define tracepoint log_trace("")

namespace xpu {
extern std::string GetContainerIdFromCgroup(const std::string &filePath);
extern void LogToFile(const std::string &logdir, std::shared_ptr<spdlog::logger> &logger);
extern void LogInit(const std::string &loggerType, const std::string &sourceId);
} // namespace xpu

#endif