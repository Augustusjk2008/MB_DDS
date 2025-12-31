/**
 * @file CanDevice.h
 * @brief 基于 XDMA 的 Xilinx AXI CAN 设备适配器（v1.03.a，FIFO 模式）
 */
#pragma once

#include <cstddef>
#include <cstdint>

#include "MB_DDF/PhysicalLayer/Device/TransportLinkAdapter.h"
#include "MB_DDF/PhysicalLayer/ControlPlane/IDeviceTransport.h"
#include "MB_DDF/PhysicalLayer/Device/CanFdDevice.h" // 复用 CanFrame 表示

namespace MB_DDF {
namespace PhysicalLayer {
namespace Device {

class CanDevice : public TransportLinkAdapter {
public:
    explicit CanDevice(MB_DDF::PhysicalLayer::ControlPlane::IDeviceTransport& tp, uint16_t mtu)
        : TransportLinkAdapter(tp, mtu) {}

    // 设备初始化：复位 -> 配置模式 -> 配置回环与位时间 -> 设定验收滤波 -> 进入正常/回环
    bool open(const LinkConfig& cfg) override;
    bool close() override;

    // 原始缓冲区语义：与 CanFDDevice 保持一致的简单序列化：
    // [id:4][flags:1][dlc:1][data:dlc]
    bool    send(const uint8_t* data, uint32_t len) override;
    int32_t receive(uint8_t* buf, uint32_t buf_size) override;
    int32_t receive(uint8_t* buf, uint32_t buf_size, uint32_t timeout_us) override;

    // 帧语义
    bool    send(CanFrame& frame);
    int32_t receive(CanFrame& frame);
    int32_t receive(CanFrame& frame, uint32_t timeout_us);

    // 设备控制：提供基本 ioctl 操作码
    enum : uint32_t {
        IOCTL_RESET            = 0x1001,
        IOCTL_SET_LOOPBACK     = 0x1002, // in: uint32_t on/off
        IOCTL_SET_BIT_TIMING   = 0x1003, // in: uint32_t baudrate (24MHz 时钟下支持 1M/500K/250K)
        IOCTL_CONFIG_FILTER_ALL= 0x1004  // 接收所有ID
    };

    struct BitTiming {
        uint8_t prescaler{1}; // BRP
        uint8_t ts1{7};       // TSEG1（示例：7）
        uint8_t ts2{2};       // TSEG2（示例：2）
        uint8_t sjw{0};       // SJW（示例：0）
    };

    int ioctl(uint32_t opcode, const void* in = nullptr, size_t in_len = 0,
              void* out = nullptr, size_t out_len = 0) override;

private:
    bool __reset();
    bool __enter_config();
    bool __set_loopback(bool on);
    bool __set_bit_timing(const BitTiming& bt);
    bool __config_filter_accept_all();
    bool __enable_core();

    bool __write_tx_fifo(const CanFrame& f);
    int  __read_rx_fifo(CanFrame& f);
};

} // namespace Device
} // namespace PhysicalLayer
} // namespace MB_DDF