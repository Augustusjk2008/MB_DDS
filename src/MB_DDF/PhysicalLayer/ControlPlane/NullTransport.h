/**
 * @file NullTransport.h
 * @brief 占位控制面实现：不提供任何硬件资源，仅用于构建设备聚合与能力探测流程
 * @date 2025-10-24
 */
#pragma once

#include "MB_DDF/PhysicalLayer/ControlPlane/IDeviceTransport.h"

namespace MB_DDF {
namespace PhysicalLayer {
namespace ControlPlane {

class NullTransport : public IDeviceTransport {
public:
    bool open(const TransportConfig& cfg) override {
        (void)cfg;
        return true; // 始终成功，便于上层流程测试
    }
    void close() override {}

    void*  getMappedBase() const override { return nullptr; }
    size_t getMappedLength() const override { return 0; }
    bool   readReg32(uint64_t, uint32_t&) const override { return false; }
    bool   writeReg32(uint64_t, uint32_t) override { return false; }

    int    waitEvent(uint32_t*, uint32_t) override { return 0; }
    int    getEventFd() const override { return -1; }

    bool   dmaWrite(int, const void*, size_t) override { return false; }
    bool   dmaRead(int, void*, size_t) override { return false; }
    void   setOnDmaWriteComplete(std::function<void(ssize_t)> cb) override { (void)cb; }
    void   setOnDmaReadComplete(std::function<void(ssize_t)> cb) override { (void)cb; }

    bool   dmaWriteAsync(int, const void*, size_t, uint64_t) override { return false; }
    bool   dmaReadAsync(int, void*, size_t, uint64_t) override { return false; }
};

} // namespace ControlPlane
} // namespace PhysicalLayer
} // namespace MB_DDF