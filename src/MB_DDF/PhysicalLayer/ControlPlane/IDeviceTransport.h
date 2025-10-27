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
    virtual bool   readReg32(uint64_t offset, uint32_t& val) const = 0;
    virtual bool   writeReg32(uint64_t offset, uint32_t val) = 0;

    // 事件等待与 fd 暴露
    virtual int    waitEvent(uint32_t* bitmap, uint32_t timeout_ms) = 0; // >0 事件号；0 超时；<0 错误
    virtual int    getEventFd() const { return -1; }   // <0 表示不支持 fd 事件模型

    // DMA 接口（可选，由具体实现决定是否支持）
    virtual bool   dmaWrite(int channel, const void* buf, size_t len) = 0;
    virtual bool   dmaRead(int channel, void* buf, size_t len) = 0;

    // 全局异步完成回调设置
    virtual void   setOnDmaWriteComplete(std::function<void(ssize_t)> cb) = 0;
    virtual void   setOnDmaReadComplete(std::function<void(ssize_t)> cb) = 0;

    // 异步 DMA 发起，不再逐次传入回调
    virtual bool   dmaWriteAsync(int channel, const void* buf, size_t len, uint64_t device_offset) = 0;
    virtual bool   dmaReadAsync(int channel, void* buf, size_t len, uint64_t device_offset) = 0;
};

} // namespace ControlPlane
} // namespace PhysicalLayer
} // namespace MB_DDF