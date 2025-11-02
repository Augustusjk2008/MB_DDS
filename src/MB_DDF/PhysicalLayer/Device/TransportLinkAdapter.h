/**
 * @file TransportLinkAdapter.h
 * @brief 将控制面 IDeviceTransport 适配为数据面 ILink（阶段 B）
 * @date 2025-10-24
 * 
 * 适配策略：
 * - send/receive 映射到 DMA 通道或映射队列；
 * - 事件 fd 直接复用控制面的事件 fd；
 * - MTU/状态从配置与能力推导。
 */
#pragma once

#include <cstddef>
#include <cstdint>

#include "MB_DDF/PhysicalLayer/DataPlane/ILink.h"
#include "MB_DDF/PhysicalLayer/ControlPlane/IDeviceTransport.h"

namespace MB_DDF {
namespace PhysicalLayer {
namespace Device {

class TransportLinkAdapter : public DataPlane::ILink {
public:
    // 通过构造参数指定数据面 MTU 与映射的 DMA 通道编号
    explicit TransportLinkAdapter(ControlPlane::IDeviceTransport& tp, uint16_t mtu)
        : tp_(tp), mtu_(mtu) {}

    bool open(const LinkConfig& cfg) override {
        (void)cfg; // 链路层配置主要由设备控制面决定
        status_ = LinkStatus::OPEN;
        return true;
    }
    bool close() override {
        status_ = LinkStatus::CLOSED;
        return true;
    }

    virtual bool send(const uint8_t* data, uint32_t len) override = 0;
    virtual int32_t receive(uint8_t* buf, uint32_t buf_size) override = 0;
    virtual int32_t receive(uint8_t* buf, uint32_t buf_size, uint32_t timeout_us) override = 0;

    LinkStatus getStatus() const override { return status_; }
    uint16_t   getMTU() const override { return mtu_; }

    int getEventFd() const override { return tp_.getEventFd(); }
    int getIOFd() const override { return -1; }
    
    ControlPlane::IDeviceTransport& transport() { return tp_; }
    const ControlPlane::IDeviceTransport& transport() const { return tp_; }

    // 纯虚：适配器的具体设备控制由派生类实现（寄存器读写集等）
    int ioctl(uint32_t opcode, const void* in, size_t in_len, void* out, size_t out_len) override = 0;

protected:    
    // 寄存器读写便捷封装
    inline bool rd8(uint64_t off, uint8_t& v) { return tp_.readReg8(off, v); }
    inline bool wr8(uint64_t off, uint8_t v) { return tp_.writeReg8(off, v); }
    inline bool rd16(uint64_t off, uint16_t& v) { return tp_.readReg16(off, v); }
    inline bool wr16(uint64_t off, uint16_t v) { return tp_.writeReg16(off, v); }
    inline bool rd32(uint64_t off, uint32_t& v) { return tp_.readReg32(off, v); }
    inline bool wr32(uint64_t off, uint32_t v) { return tp_.writeReg32(off, v); }

private:
    ControlPlane::IDeviceTransport& tp_;
    uint16_t mtu_{1500};
    LinkStatus status_{LinkStatus::CLOSED};
};

} // namespace Device
} // namespace PhysicalLayer
} // namespace MB_DDF