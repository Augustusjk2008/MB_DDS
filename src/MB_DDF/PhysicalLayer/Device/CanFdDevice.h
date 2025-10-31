/**
 * @file CanFDDevice.h
 * @brief 基于 XDMA 的 Xilinx CANFD IP 设备适配器（v2.0，FIFO 模式，RX 中断）
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "MB_DDF/PhysicalLayer/Device/TransportLinkAdapter.h"
#include "MB_DDF/PhysicalLayer/ControlPlane/IDeviceTransport.h"

namespace MB_DDF {
namespace PhysicalLayer {
namespace Device {

// 轻量帧表示：支持 CAN/CANFD，标准与扩展 ID
struct CanFrame {
    uint32_t id{0};      // 标准 11 位或扩展 29 位 ID
    bool     ide{false}; // 扩展帧标志（true=29位）
    bool     rtr{false}; // 远程帧
    bool     fdf{false}; // CANFD 帧（EDL=1）
    bool     brs{false}; // 位速率切换（CANFD）
    uint8_t  dlc{0};     // DLC 0..15
    std::vector<uint8_t> data; // 0..64 字节
};

class CanFDDevice : public TransportLinkAdapter {
public:
    explicit CanFDDevice(MB_DDF::PhysicalLayer::ControlPlane::IDeviceTransport& tp,
                         uint16_t mtu,
                         int h2c_ch = -1,
                         int c2h_ch = -1)
        : TransportLinkAdapter(tp, mtu, h2c_ch, c2h_ch) {}

    // 设备初始化：复位 -> 配置模式 -> 设定 RX FIFO 水位/过滤 -> 进入正常模式
    bool open(const LinkConfig& cfg) override;
    bool close() override;

    // 发送/接收以帧语义实现；send/receive 面向原始缓冲区时，假定为单帧序列化
    bool    send(const uint8_t* data, uint32_t len) override;
    int32_t receive(uint8_t* buf, uint32_t buf_size) override;
    int32_t receive(uint8_t* buf, uint32_t buf_size, uint32_t timeout_us) override;

protected:
    int ioctl(uint32_t opcode, const void* in, size_t in_len, void* out, size_t out_len) override;

private:
    // 寄存器读写便捷封装
    inline bool rd32(uint64_t off, uint32_t& v) { return transport().readReg32(off, v); }
    inline bool wr32(uint64_t off, uint32_t v) { return transport().writeReg32(off, v); }

    // DLC 到字节长度映射（支持 CAN/CANFD 常用编码）
    static uint8_t dlc_to_len(uint8_t dlc);
    static uint8_t len_to_dlc(uint8_t len);

    // 帧序列化到 TX FIFO（基址为 0x0100 起）；返回是否成功
    bool write_one_frame_to_txfifo(const CanFrame& f);
    // 从 RX FIFO 读取一帧（FIFO0）；返回是否成功并填充 out
    bool read_one_frame_from_rxfifo(CanFrame& out);

    // 触发发送：将任意可用 TX 缓冲标记为准备好（简化为全掩码）
    bool trigger_transmit();
};

} // namespace Device
} // namespace PhysicalLayer
} // namespace MB_DDF