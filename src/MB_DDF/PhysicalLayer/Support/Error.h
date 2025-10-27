/**
 * @file Error.h
 * @brief 错误码与返回值约定的辅助宏/函数
 * @date 2025-10-24
 */
#pragma once

#include <cerrno>

namespace MB_DDF {
namespace PhysicalLayer {
namespace Support {

// 统一返回语义建议：
// - bool：成功/失败；失败时可结合 errno 或自定义 code。
// - int：事件等待/接收类：>0 表示有效数量/事件；0 表示超时或无数据；<0 表示错误。
// - 错误码域：优先使用标准 errno（如 EIO/ETIME/EINVAL/ENODEV），必要时附加自定义域。

inline int as_timeout() { return 0; }
inline int as_error() { return -1; }

} // namespace Support
} // namespace PhysicalLayer
} // namespace MB_DDF