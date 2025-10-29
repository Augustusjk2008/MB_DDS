/**
 * @file SpiTransport.h
 * @brief 基于 spidev 的控制面实现；事件通过 GPIO 中断（libgpiod）
 */
#pragma once

#include "MB_DDF/PhysicalLayer/ControlPlane/IDeviceTransport.h"
#include <cstdint>
#include <cstddef>
#include <functional>

#include <linux/spi/spidev.h>

// libgpiod 条件包含（v1/v2 均可）；缺失时保持可编译但禁用事件
#if __has_include(<gpiod.h>)
#include <gpiod.h>
#ifndef MB_DDF_HAS_GPIOD
#define MB_DDF_HAS_GPIOD 1
#endif
#else
#ifndef MB_DDF_HAS_GPIOD
#define MB_DDF_HAS_GPIOD 0
#endif
struct gpiod_chip; // 前向声明以允许成员指针存在
struct gpiod_line; // 前向声明以允许成员指针存在
#endif

namespace MB_DDF {
namespace PhysicalLayer {
namespace ControlPlane {

class SpiTransport : public IDeviceTransport {
public:
    SpiTransport() = default;
    ~SpiTransport() override { close(); }

    bool open(const TransportConfig& cfg) override;
    void close() override;

    // 无 mmap 寄存器访问
    void*  getMappedBase() const override { return nullptr; }
    size_t getMappedLength() const override { return 0; }

    // 通过 SPI 进行寄存器读写（设备协议需与硬件一致，当前采用通用 offset+cmd 前导）
    bool readReg8(uint64_t offset, uint8_t& val) const override;
    bool writeReg8(uint64_t offset, uint8_t val) override;
    bool readReg16(uint64_t offset, uint16_t& val) const override;
    bool writeReg16(uint64_t offset, uint16_t val) override;
    bool readReg32(uint64_t offset, uint32_t& val) const override;
    bool writeReg32(uint64_t offset, uint32_t val) override;

    // 大块读写：SPI 未必支持偏移寻址；此处仅提供流式占位实现，默认返回 false
    bool continuousWrite(int channel, const void* buf, size_t len) override;
    bool continuousRead(int channel, void* buf, size_t len) override;
    bool continuousWriteAt(int channel, const void* buf, size_t len, uint64_t device_offset) override;
    bool continuousReadAt(int channel, void* buf, size_t len, uint64_t device_offset) override;

    // SPI 原始半双工传输
    bool xfer(const uint8_t* tx, uint8_t* rx, size_t len) override;

    // 异步接口
    void setOnContinuousWriteComplete(std::function<void(ssize_t)> cb) override { on_write_complete_ = std::move(cb); }
    void setOnContinuousReadComplete(std::function<void(ssize_t)> cb) override { on_read_complete_ = std::move(cb); }
    bool continuousWriteAsync(int channel, const void* buf, size_t len, uint64_t device_offset) override;
    bool continuousReadAsync(int channel, void* buf, size_t len, uint64_t device_offset) override;

    // 事件等待 & 事件 FD 暴露（GPIO 中断）
    int waitEvent(uint32_t* bitmap, uint32_t timeout_ms) override;
    int getEventFd() const override { return gpio_event_fd_; }

private:
    static int set_nonblock(int fd);

    // SPI 传输原语（半双工），根据 tx/rx 缓冲区进行一次传输
    bool spi_transfer(const uint8_t* tx, uint8_t* rx, size_t len) const;

    // 通用寄存器协议：cmd(1) + offset(4 LE) + payload(N)
    bool reg_read(uint64_t offset, void* out, size_t len) const;
    bool reg_write(uint64_t offset, const void* in, size_t len);


private:
    TransportConfig cfg_{};

    // SPI 设备
    int spi_fd_{-1};
    uint32_t spi_speed_{1'000'000}; // 默认 1MHz
    uint8_t  spi_mode_{SPI_MODE_0};
    uint8_t  spi_bits_{8};

    // GPIO 事件资源（在未启用 gpiod 时保持为空指针）
    gpiod_chip* chip_{nullptr};
    gpiod_line* line_{nullptr};
    int gpio_event_fd_{-1};

    // 异步完成回调
    std::function<void(ssize_t)> on_write_complete_{};
    std::function<void(ssize_t)> on_read_complete_{};
};

} // namespace ControlPlane
} // namespace PhysicalLayer
} // namespace MB_DDF