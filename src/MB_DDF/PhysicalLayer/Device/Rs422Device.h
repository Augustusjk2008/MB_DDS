/**
 * @file Rs422Device.h
 * @brief 基于 XDMA 的 RS422 设备型数据面适配器（配置走寄存器，数据走 DMA）
 */
#pragma once

#include "MB_DDF/PhysicalLayer/Device/TransportLinkAdapter.h"
#include "MB_DDF/PhysicalLayer/ControlPlane/IDeviceTransport.h"

namespace MB_DDF {
namespace PhysicalLayer {
namespace Device {

class Rs422Device : public MB_DDF::PhysicalLayer::Device::TransportLinkAdapter {
public:
    // DMA 通道与事件均按 0 配置；MTU 可由上层配置传入
    explicit Rs422Device(MB_DDF::PhysicalLayer::ControlPlane::IDeviceTransport& tp, uint16_t mtu)
        : TransportLinkAdapter(tp, mtu, /*h2c*/-1, /*c2h*/-1) {}

    // ioctl 操作码：设备配置（参考 rs422_config）
    static constexpr uint32_t IOCTL_CONFIG = 0x01;

    // 设备配置数据结构（直接对应寄存器值，逻辑清晰）
    // - ucr: UART 控制寄存器值（参考: parity/check 等编码）
    // - mcr: 模式控制寄存器值（参考驱动默认 0x20）
    // - brsr: 波特率选择寄存器值
    // - icr: 中断/状态控制寄存器值（通常写 1 以启用状态）
    // - tx_head_lo/hi: 发送头两个字节（低/高）
    // - rx_head_lo/hi: 接收头两个字节（低/高）
    struct Config {
        uint8_t ucr{0};
        uint8_t mcr{0x20};
        uint8_t brsr{0};
        uint8_t icr{1};
        uint8_t tx_head_lo{0};
        uint8_t tx_head_hi{0};
        uint8_t rx_head_lo{0};
        uint8_t rx_head_hi{0};
    };

    bool open(const LinkConfig& cfg) override;
    bool close() override;

    // 重载：基于寄存器的收发，不使用 DMA
    bool    send(const uint8_t* data, uint32_t len, const DataPlane::Endpoint& dst) override;
    int32_t receive(uint8_t* buf, uint32_t buf_size, DataPlane::Endpoint& src) override;
    int32_t receive(uint8_t* buf, uint32_t buf_size, DataPlane::Endpoint& src, uint32_t timeout_us) override;

    // ioctl 留空实现（待用户后续补充具体寄存器配置/查询语义）
    int ioctl(uint32_t opcode, const void* in, size_t in_len, void* out, size_t out_len) override;
};

} // namespace Device
} // namespace PhysicalLayer
} // namespace MB_DDF