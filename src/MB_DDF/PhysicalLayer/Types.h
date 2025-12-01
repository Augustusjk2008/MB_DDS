/**
 * @file Types.h
 * @brief PhysicalLayer 公共类型与配置定义（阶段 A）
 * @date 2025-10-24
 * 
 * 说明：
 * - 面向 Linux “设备即文件”模型的抽象，不依赖具体后端实现。
 * - 提供数据面/控制面通用的状态、能力与配置结构。
 */
#pragma once

#include <cstdint>
#include <string>

namespace MB_DDF {
namespace PhysicalLayer {

// 链路状态：统一的数据面状态表达
enum class LinkStatus {
    CLOSED = 0,
    OPEN   = 1,
    ERROR  = 2
};

// 数据面配置：保持通用/后端无关；具体后端可扩展或忽略部分字段
// 数据面配置：用于初始化 ILink；name 可承载简易地址/端口参数
struct LinkConfig {
    uint16_t mtu{1500};              // 最大传输单元（字节）
    std::string name;                // 链路名称/标识（可选）
    int channel_id{-1};              // 对于通道化后端（如队列/通道）可使用；-1 表示未指定
};

// 控制面配置：设备路径与事件/通道编号等（保持后端无关的广义表达）
struct TransportConfig {
    std::string device_path{"/dev/xdma0"};  // 设备文件/基路径（默认xdma，可选spi等）
    int dma_h2c_channel{-1};         // 主机到设备通道编号；<0 表示未启用
    int dma_c2h_channel{-1};         // 设备到主机通道编号；<0 表示未启用
    int event_number{-1};            // 事件设备编号；<0 表示未启用
    __off_t device_offset{0};        // 设备内存偏移（用于寄存器映射）
};

// 设备能力：用于运行时能力发现与上层决策，暂不实现
/*
struct DeviceCapabilities {
    bool supports_dma{false};
    bool supports_event_fd{false};
    bool has_mmap_regs{false};
    uint16_t max_frame_size{1500};

    enum class AddressingMode {
        None,        // 无特定寻址（点到点/已绑定）
        SocketAddr,  // 套接字地址（UDP/CAN 等）
        ChannelId,   // 通道/队列编号
        OffsetBased  // 偏移寻址（寄存器/环形队列）
    } addressing_mode{AddressingMode::None};
}; */

// 设备聚合配置：统一入口，包含数据面与控制面配置
struct DeviceConfig {
    LinkConfig      link;      // 数据面配置
    TransportConfig transport; // 控制面配置
};

} // namespace PhysicalLayer
} // namespace MB_DDF