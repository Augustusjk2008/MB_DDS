/**
 * @file Log.h
 * @brief 统一日志约定：[backend] op result code detail
 * @date 2025-10-24
 */
#pragma once

#include <cstdio>
#include <cstdarg>
#include <cstring>

namespace MB_DDF {
namespace PhysicalLayer {
namespace Support {

inline void vlogf(const char* level, const char* backend, const char* op, int code, const char* fmt, va_list ap) {
    std::fprintf(stderr, "[%s][%s] %s code=%d ", level, backend ? backend : "-", op ? op : "-", code);
    if (fmt && *fmt) {
        std::vfprintf(stderr, fmt, ap);
    }
    std::fprintf(stderr, "\n");
}

inline void logf(const char* level, const char* backend, const char* op, int code, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vlogf(level, backend, op, code, fmt, ap);
    va_end(ap);
}

#define LOGI(backend, op, code, fmt, ...) ::MB_DDF::PhysicalLayer::Support::logf("I", backend, op, code, fmt, ##__VA_ARGS__)
#define LOGW(backend, op, code, fmt, ...) ::MB_DDF::PhysicalLayer::Support::logf("W", backend, op, code, fmt, ##__VA_ARGS__)
#define LOGE(backend, op, code, fmt, ...) ::MB_DDF::PhysicalLayer::Support::logf("E", backend, op, code, fmt, ##__VA_ARGS__)

} // namespace Support
} // namespace PhysicalLayer
} // namespace MB_DDF