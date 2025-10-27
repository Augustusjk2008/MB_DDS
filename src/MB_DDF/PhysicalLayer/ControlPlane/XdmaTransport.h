#pragma once
#include "MB_DDF/PhysicalLayer/ControlPlane/IDeviceTransport.h"
#include <cstdint>
#include <string>

// 可选：io_uring 与 libaio 支持（默认关闭，CMake 自动探测开启）
#ifndef MB_DDF_HAS_IOURING
#define MB_DDF_HAS_IOURING 0
#endif
#ifndef MB_DDF_HAS_LIBAIO
#define MB_DDF_HAS_LIBAIO 0
#endif

#if MB_DDF_HAS_IOURING
#include <liburing.h>
#elif MB_DDF_HAS_LIBAIO
#include <libaio.h>
#endif

namespace MB_DDF {
namespace PhysicalLayer {
namespace ControlPlane {

class XdmaTransport : public IDeviceTransport {
public:
    XdmaTransport() = default;
    ~XdmaTransport() override { close(); }

    bool open(const TransportConfig& cfg) override;
    void close() override;

    // mmap 寄存器访问
    void* getMappedBase() const override { return user_base_; }
    size_t getMappedLength() const override { return mapped_len_; }
    bool readReg32(uint64_t offset, uint32_t& val) const override;
    bool writeReg32(uint64_t offset, uint32_t val) override;

    // 同步 DMA 接口
    bool dmaWrite(int channel, const void* buf, size_t len) override;
    bool dmaRead(int channel, void* buf, size_t len) override;
    // 带设备偏移的同步接口
    bool dmaWriteAt(int channel, const void* buf, size_t len, uint64_t device_offset);
    bool dmaReadAt(int channel, void* buf, size_t len, uint64_t device_offset);
    // 可选：设置默认设备偏移
    void setDefaultDeviceOffset(uint64_t off);
    void clearDefaultDeviceOffset();

    // 事件等待
    int waitEvent(uint32_t* bitmap, uint32_t timeout_ms) override;
    int getEventFd() const override { return events_fd_; }

    // 全局异步完成回调设置
    void setOnDmaWriteComplete(std::function<void(ssize_t)> cb) override { on_write_complete_ = std::move(cb); }
    void setOnDmaReadComplete(std::function<void(ssize_t)> cb) override { on_read_complete_ = std::move(cb); }

    // 异步 DMA 接口（优先 io_uring，其次 libaio）
    bool dmaWriteAsync(int channel, const void* buf, size_t len, uint64_t device_offset) override;
    bool dmaReadAsync(int channel, void* buf, size_t len, uint64_t device_offset) override;

    // 完成收割 & 事件 FD 暴露（用于与上层 event loop 集成）
    int getAioEventFd() const;
    int drainAioCompletions(int max_events);

    // 工具函数：构造设备节点路径
    static std::string make_user_path(const std::string& base) { return base + "_user"; }
    static std::string make_h2c_path(const std::string& base, int ch) { return base + "_h2c_" + std::to_string(ch); }
    static std::string make_c2h_path(const std::string& base, int ch) { return base + "_c2h_" + std::to_string(ch); }
    static std::string make_events_path(const std::string& base, int num) { return base + "_events_" + std::to_string(num); }

private:
    static long page_size();
    static int set_nonblock(int fd);

    TransportConfig cfg_{};

    // user 寄存器设备
    int user_fd_ = -1;
    void* user_base_ = nullptr;
    size_t mapped_len_ = 0;

    // DMA 设备（可选）
    int h2c_fd_ = -1;
    int c2h_fd_ = -1;

    // 事件设备（可选）
    int events_fd_ = -1;

    // 默认偏移（作用于 dmaWrite/dmaRead）
    uint64_t default_device_offset_ = 0;
    bool use_default_device_offset_ = false;

    // AIO/IOURING 资源
    int aio_event_fd_ = -1; // 统一暴露给上层的事件fd
    uint64_t next_aio_id_ = 1; // 生成唯一 user_data 键
    std::function<void(ssize_t)> on_write_complete_ = nullptr;
    std::function<void(ssize_t)> on_read_complete_ = nullptr;

#if MB_DDF_HAS_IOURING
    struct io_uring ring_{};
    bool iouring_inited_ = false;
#elif MB_DDF_HAS_LIBAIO
    io_context_t aio_ctx_ = 0; // libaio 使用 io_context_t
#endif
};

} // namespace ControlPlane
} // namespace PhysicalLayer
} // namespace MB_DDF