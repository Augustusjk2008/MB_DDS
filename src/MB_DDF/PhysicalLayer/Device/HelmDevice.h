/**
 * @file HelmDevice.h
 * @brief 基于 XDMA 的舵机（Helm）设备型适配器：send=PWM、receive=FDB、ioctl=HELM 配置
 */
#pragma once

#include <cstddef>
#include <cstdint>

#include "MB_DDF/PhysicalLayer/Device/TransportLinkAdapter.h"
#include "MB_DDF/PhysicalLayer/ControlPlane/IDeviceTransport.h"

namespace MB_DDF {
namespace PhysicalLayer {
namespace Device {

class HelmDevice : public MB_DDF::PhysicalLayer::Device::TransportLinkAdapter {
public:
    // DMA 通道不使用；MTU 可由上层配置传入（建议 >= 16 以容纳 4 路 PWM）
    explicit HelmDevice(MB_DDF::PhysicalLayer::ControlPlane::IDeviceTransport& tp, uint16_t mtu)
        : TransportLinkAdapter(tp, mtu, /*h2c*/-1, /*c2h*/-1) {}

    // ioctl 操作码（保持与 reference/axi 驱动一致，便于上层统一语义）
    static constexpr uint32_t IOCTL_FDB  = 0x9077; // 舵机 AD 反馈读取
    static constexpr uint32_t IOCTL_PWM  = 0x9078; // 舵机 PWM 占空比设置
    static constexpr uint32_t IOCTL_HELM = 0x9079; // 舵机模块初始化/配置

    // 舵机配置参数（参考 reference/axi/_HELM_CFG）
    struct Config {
        uint16_t pwm_freq{0};   // PWM 频率参数
        uint16_t out_enable{0}; // PWM 输出使能位图（bit0..3 对应 4 路）
        uint16_t ad_filter{0};  // AD 反馈滤波参数
    };

    bool open(const LinkConfig& cfg) override;
    bool close() override;

    // send: 实现 ioctlPwm —— 将 4 路 32 位占空比写入寄存器
    bool    send(const uint8_t* data, uint32_t len) override;

    // receive: 实现 ioctlFdb —— 读取 4 路 16 位 AD 反馈并按小端序写入 buf
    int32_t receive(uint8_t* buf, uint32_t buf_size) override;

    // 带 timeout 的 receive 直接调用非阻塞 receive（按需求简化）
    int32_t receive(uint8_t* buf, uint32_t buf_size, uint32_t timeout_us) override;

    // ioctl: 实现 ioctlHelm —— 根据配置参数完成初始化与输出使能设置
    int ioctl(uint32_t opcode, const void* in, size_t in_len, void* out, size_t out_len) override;

private:
    bool initialized_{false};
};

} // namespace Device
} // namespace PhysicalLayer
} // namespace MB_DDF