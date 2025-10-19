/**
 * @file PcieTransport.h
 * @brief 基于 Linux sysfs 的 PCIe 传输实现（用户态）
 * @date 2025-10-18
 */

#pragma once

#include "MB_DDF/PhysicalLayer/Transport/ITransport.h"

namespace MB_DDF {
namespace PhysicalLayer {
namespace Transport {

/**
 * @class PcieTransport
 * @brief 使用 /sys/bus/pci/devices/.../resourceN 映射 BAR 的传输实现
 * 
 * 特性：
 * - 支持根据 BDF(Bus:Device.Function) 自动定位 resourceN 文件；
 * - 解析 resource 文件获得 BAR 尺寸；
 * - 提供 32 位寄存器读写接口；
 * - 事件等待暂未实现（返回 0 表示超时）
 */
class PcieTransport : public ITransport {
public:
    PcieTransport() = default;
    ~PcieTransport() override { close(); }

    bool open(const TransportConfig& cfg) override;
    void close() override;

    void*  getMappedBase() const override { return mapped_base_; }
    size_t getMappedLength() const override { return mapped_len_; }

    bool readReg32(uint64_t offset, uint32_t& val) const override;
    bool writeReg32(uint64_t offset, uint32_t val) override;

    int waitEvent(uint32_t timeout_ms) override;

private:
    int fd_{-1};
    void* mapped_base_{nullptr};
    size_t mapped_len_{0};

    TransportConfig cfg_{};

    bool map_bar_via_sysfs(const PcieAddress& addr);
    static bool get_bar_size_from_resource_table(const PcieAddress& addr, size_t& out_size);
};

} // namespace Transport
} // namespace PhysicalLayer
} // namespace MB_DDF