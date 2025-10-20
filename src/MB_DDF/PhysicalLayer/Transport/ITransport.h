/**
 * @file ITransport.h
 * @brief 传输层抽象接口（支持 PCIE 等底层物理接口）
 * @date 2025-10-18
 * 
 * 设计目标：
 * - 为上层设备（如 UART over PCIE）提供统一的底层读写与事件等待接口；
 * - 便于扩展不同后端（PCIE、UIO/VFIO、XDMA、自定义）；
 * - 与现有 PhysicalLayer 的 Address/LinkConfig 解耦，避免侵入式修改。
 */

#pragma once

#include <cstdint>
#include <string>
#include <cstddef>

namespace MB_DDF {
namespace PhysicalLayer {
namespace Transport {

/**
 * @enum Backend
 * @brief 传输层后端类型
 */
enum class Backend {
    PCIE,   ///< 通过标准 Linux PCIe sysfs 资源文件（resourceN）映射 BAR
    UIO,    ///< 通过 /dev/uioX 访问（用户态中断 + 映射）
    XDMA,   ///< 通过厂商 XDMA 设备节点访问（H2C/C2H + user BAR）
    CUSTOM  ///< 其他自定义后端
};

/**
 * @struct PcieAddress
 * @brief PCIE 设备寻址与 BAR 映射配置
 */
struct PcieAddress {
    uint16_t domain{0};     ///< PCI 域，一般为 0
    uint8_t  bus{0};        ///< 总线号
    uint8_t  device{0};     ///< 设备号
    uint8_t  function{0};   ///< 功能号
    int      bar_index{0};  ///< 选择映射的 BAR 序号（resourceN 中的 N）
    size_t   bar_size{0};   ///< 期望映射长度（0 表示根据 resourceN 大小自动推断）

    // 可选：直接指定资源文件路径（优先级高于 domain/bus/dev.fn）
    std::string resource_path;

    std::string to_resource_path() const;
};

/**
 * @struct TransportConfig
 * @brief 统一的传输配置
 * 
 * 字段说明：
 * - backend: 选择后端实现（PCIE/XDMA/…）。
 * - device_path: 对于 XDMA，建议使用基路径，如 "/dev/xdma0"，
 *   实现会自动派生 "_user"、"_h2c_<ch>"、"_c2h_<ch>"、"_events_<n>"。
 * - dma_h2c_channel/dma_c2h_channel: XDMA 的 DMA 通道编号，<0 表示不打开。
 * - event_number: XDMA 的事件设备编号（0-15），<0 表示不打开事件设备。
 */
struct TransportConfig {
    Backend backend{Backend::PCIE};
    PcieAddress pcie;            ///< 当 backend==PCIE 时使用

    // 预留扩展字段：如 UIO/XDMA 的设备路径、DMA 通道号等
    std::string device_path;     ///< 后端设备路径（可选，XDMA 推荐基路径，如 "/dev/xdma0"）
    int dma_h2c_channel{-1};     ///< 主机到卡（H2C）通道（可选，<0 表示禁用）
    int dma_c2h_channel{-1};     ///< 卡到主机（C2H）通道（可选，<0 表示禁用）
    int event_number{-1};        ///< XDMA 事件设备编号（0-15），<0 表示不启用事件
};

/**
 * @class ITransport
 * @brief 底层传输抽象接口
 */
class ITransport {
public:
    virtual ~ITransport() = default;

    // 生命周期
    virtual bool open(const TransportConfig& cfg) = 0;
    virtual void close() = 0;

    // 映射与寄存器读写（以 32bit 为主，满足绝大多数寄存器需求）
    virtual void*  getMappedBase() const = 0;      ///< 返回 BAR 映射基址（nullptr 表示未映射）
    virtual size_t getMappedLength() const = 0;    ///< 返回映射长度

    virtual bool readReg32(uint64_t offset, uint32_t& val) const = 0;
    virtual bool writeReg32(uint64_t offset, uint32_t val) = 0;

    // 事件等待（如中断/epoll 等）。未实现则返回 0 表示超时，<0 表示错误，>0 表示事件号
    virtual int waitEvent(uint32_t timeout_ms) = 0;

    // DMA（可选扩展）：默认由具体后端决定是否支持
    virtual bool dmaWrite(int channel, const void* buf, size_t len) { (void)channel; (void)buf; (void)len; return false; }
    virtual bool dmaRead(int channel, void* buf, size_t len) { (void)channel; (void)buf; (void)len; return false; }
};

} // namespace Transport
} // namespace PhysicalLayer
} // namespace MB_DDF