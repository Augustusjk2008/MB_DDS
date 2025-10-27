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
    explicit TransportLinkAdapter(ControlPlane::IDeviceTransport& tp, uint16_t mtu, int h2c_ch, int c2h_ch)
        : tp_(tp), mtu_(mtu), h2c_ch_(h2c_ch), c2h_ch_(c2h_ch) {}

    bool open(const LinkConfig& cfg) override {
        (void)cfg; // 链路层配置主要由设备控制面决定
        status_ = LinkStatus::OPEN;
        return true;
    }
    bool close() override {
        status_ = LinkStatus::CLOSED;
        return true;
    }

    bool send(const uint8_t* data, uint32_t len, const DataPlane::Endpoint& dst) override {
        (void)dst; // 设备型后端通常不需要端点（或用队列号）
        return tp_.dmaWriteAsync(h2c_ch_, data, len, device_offset_);
    }

    int32_t receive(uint8_t* buf, uint32_t buf_size, DataPlane::Endpoint& src) override {
        (void)src;
        bool ok = tp_.dmaReadAsync(c2h_ch_, buf, buf_size, device_offset_);
        if (!ok) return 0; // 无数据或错误以 0/负数区分，此处简化为无数据
        // 注意：若需要精确字节数，需设备侧协议支持；这里返回缓冲区大小的最小值
        return static_cast<int32_t>(buf_size);
    }

    int32_t receive(uint8_t* buf, uint32_t buf_size, DataPlane::Endpoint& src, uint32_t timeout_us) override {
        uint32_t bitmap = 0;
        int ev = tp_.waitEvent(&bitmap, timeout_us / 1000);
        if (ev <= 0) return ev == 0 ? 0 : -1; // 0 超时；-1 错误
        return receive(buf, buf_size, src);
    }

    LinkStatus getStatus() const override { return status_; }
    uint16_t   getMTU() const override { return mtu_; }

    int getEventFd() const override { return tp_.getEventFd(); }
    int getIOFd() const override { return -1; }

protected:
    ControlPlane::IDeviceTransport& transport() { return tp_; }
    const ControlPlane::IDeviceTransport& transport() const { return tp_; }

    // 纯虚：适配器的具体设备控制由派生类实现（寄存器读写集等）
    int ioctl(uint32_t opcode, const void* in, size_t in_len, void* out, size_t out_len) override = 0;

private:
    ControlPlane::IDeviceTransport& tp_;
    uint64_t device_offset_{0};
    uint16_t mtu_{1500};
    int h2c_ch_{-1};
    int c2h_ch_{-1};
    LinkStatus status_{LinkStatus::CLOSED};
};

} // namespace Device
} // namespace PhysicalLayer
} // namespace MB_DDF