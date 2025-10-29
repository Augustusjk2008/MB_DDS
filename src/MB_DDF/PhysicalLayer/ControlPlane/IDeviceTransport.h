/**
 * @file IDeviceTransport.h
 * @brief 控制面抽象接口：寄存器/DMA/事件（阶段 A）
 * @date 2025-10-24
 * 
 * 设计要点：
 * - 面向 Linux 设备文件语义，优先暴露可集成到 epoll 的 fd。
 * - 保持对寄存器与 DMA 的统一抽象，不绑定具体后端实现。
 */
#pragma once

#include "MB_DDF/PhysicalLayer/Types.h"
#include <cstdint>
#include <cstddef>
#include <functional>

namespace MB_DDF {
namespace PhysicalLayer {
namespace ControlPlane {

class IDeviceTransport {
public:
    virtual ~IDeviceTransport() = default;

    // 生命周期
    virtual bool open(const TransportConfig& cfg) = 0;
    virtual void close() = 0;

    // 映射与寄存器访问
    // 返回 mmap 的用户寄存器映射基址；用于 readReg32/writeReg32 或更复杂数据结构访问
    virtual void*  getMappedBase() const = 0;   // nullptr 表示未映射
    virtual size_t getMappedLength() const = 0; // 0 表示未映射
    virtual bool   readReg8(uint64_t offset, uint8_t& val) const = 0;
    virtual bool   writeReg8(uint64_t offset, uint8_t val) = 0;
    virtual bool   readReg16(uint64_t offset, uint16_t& val) const = 0;
    virtual bool   writeReg16(uint64_t offset, uint16_t val) = 0;
    virtual bool   readReg32(uint64_t offset, uint32_t& val) const = 0;
    virtual bool   writeReg32(uint64_t offset, uint32_t val) = 0;

    // 原始半双工传输接口（SPI 类），tx/rx 至少一个非空，len>0
    // 非 SPI 的实现可直接返回 false 表示不支持
    virtual bool   xfer(const uint8_t* tx, uint8_t* rx, size_t len) = 0;

    // 事件等待与 fd 暴露
    virtual int    waitEvent(uint32_t* bitmap, uint32_t timeout_ms) = 0; // >0 事件号；0 超时；<0 错误
    virtual int    getEventFd() const { return -1; }   // <0 表示不支持 fd 事件模型

    // 大块读写接口（可选，由具体实现决定是否支持）
    virtual bool   continuousWrite(int channel, const void* buf, size_t len) = 0;
    virtual bool   continuousRead(int channel, void* buf, size_t len) = 0;
    virtual bool   continuousWriteAt(int channel, const void* buf, size_t len, uint64_t device_offset) = 0;
    virtual bool   continuousReadAt(int channel, void* buf, size_t len, uint64_t device_offset) = 0;

    // 全局异步完成回调设置
    virtual void   setOnContinuousWriteComplete(std::function<void(ssize_t)> cb) = 0;
    virtual void   setOnContinuousReadComplete(std::function<void(ssize_t)> cb) = 0;

    // 异步大块读写发起，不再逐次传入回调
    virtual bool   continuousWriteAsync(int channel, const void* buf, size_t len, uint64_t device_offset) = 0;
    virtual bool   continuousReadAsync(int channel, void* buf, size_t len, uint64_t device_offset) = 0;
};

// 统一大小端转换（以小端为设备字节序）
inline uint16_t htol_u16(uint16_t x) {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return __builtin_bswap16(x);
#else
    return x;
#endif
}
inline uint16_t ltoh_u16(uint16_t x) {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return __builtin_bswap16(x);
#else
    return x;
#endif
}
inline uint32_t htol_u32(uint32_t x) {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return __builtin_bswap32(x);
#else
    return x;
#endif
}
inline uint32_t ltoh_u32(uint32_t x) {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return __builtin_bswap32(x);
#else
    return x;
#endif
}

} // namespace ControlPlane
} // namespace PhysicalLayer
} // namespace MB_DDF