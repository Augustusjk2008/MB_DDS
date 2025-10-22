/**
 * @file XdmaTransport.h
 * @brief 基于 Linux XDMA 驱动设备节点的传输实现
 * @date 2025-10-19
 * 
 * 仅支持 Linux，按照参考 C 程序接口移植：
 * - 寄存器读写：通过 `device_path + "_user"` 设备进行 `mmap` 映射，提供 8/16/32 位访问；
 * - DMA H2C/C2H：使用 `device_path + "_h2c_<ch>"` 与 `device_path + "_c2h_<ch>"` 的文件读写；
 * - 事件等待：可选打开 `device_path + "_events_<n>"`，阻塞 `read` 获取中断状态；
 * 
 * 性能考虑：
 * - 寄存器访问采用持久映射（一次 mmap，重复读写），避免每次调用映射开销；
 * - DMA 读写内部按参考实现使用分块循环读写，支持大数据量；
 * - 提供 `dmaWrite`/`dmaRead` 高层接口，同时保留 `waitEvent` 简易事件等待。
 */

#pragma once

#include "MB_DDF/PhysicalLayer/Transport/ITransport.h"
#include <cstdint>
#include <cstddef>
#include <string>

namespace MB_DDF {
namespace PhysicalLayer {
namespace Transport {

class XdmaTransport : public ITransport {
public:
    XdmaTransport() = default;
    ~XdmaTransport() override { close(); }

    bool open(const TransportConfig& cfg) override;
    void close() override;

    void*  getMappedBase() const override { return user_base_; }
    size_t getMappedLength() const override { return mapped_len_; }

    bool readReg32(uint64_t offset, uint32_t& val) const override;
    bool writeReg32(uint64_t offset, uint32_t val) override;

    // 额外提供 8/16 位访问以匹配参考程序功能
    bool readReg8(uint64_t offset, uint8_t& val) const;
    bool readReg16(uint64_t offset, uint16_t& val) const;
    bool writeReg8(uint64_t offset, uint8_t val);
    bool writeReg16(uint64_t offset, uint16_t val);
    int waitEvent(uint32_t timeout_ms) override;
    int getEventFd() const override;

    bool dmaWrite(int channel, const void* buf, size_t len) override;
    bool dmaRead(int channel, void* buf, size_t len) override;

    // 兼容参考程序：支持指定设备偏移与缓冲区偏移的 DMA 接口
    bool dmaWriteAt(int channel, const void* buf, size_t len, uint64_t dev_offset, size_t buf_offset);
    bool dmaReadAt(int channel, void* buf, size_t len, uint64_t dev_offset, size_t buf_offset);

private:
    // 设备文件与句柄
    std::string base_path_{};   // 例如 "/dev/xdma0"
    int user_fd_{-1};           // 寄存器访问（_user）
    int h2c_fd_{-1};            // 主机到卡（_h2c_<ch>）
    int c2h_fd_{-1};            // 卡到主机（_c2h_<ch>）
    int events_fd_{-1};         // 事件设备（_events_<n>）

    // 持久映射
    void*  user_base_{nullptr}; // _user 设备的映射基址
    size_t mapped_len_{0};

    // 配置缓存
    TransportConfig cfg_{};

    // 帮助函数
    static std::string make_user_path(const std::string& base);
    static std::string make_h2c_path(const std::string& base, int ch);
    static std::string make_c2h_path(const std::string& base, int ch);
    static std::string make_events_path(const std::string& base, int num);

    static size_t page_size();
};

} // namespace Transport
} // namespace PhysicalLayer
} // namespace MB_DDF