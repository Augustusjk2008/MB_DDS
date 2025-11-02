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
    bool     esi{false}; // 错误状态指示（CANFD）
    uint8_t  dlc{0};     // DLC 0..15
    uint8_t  len{0};     // 数据长度 0..64 字节
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

    // 发送接收帧语义实现
    bool send(CanFrame& frame);
    int32_t receive(CanFrame& frame);

    // 设备控制：配置参数、查询状态
    int ioctl(uint32_t opcode, const void* in, size_t in_len, void* out, size_t out_len) override;

private:
    // DLC 到字节长度映射（支持 CAN/CANFD 常用编码）
    static uint8_t dlc_to_len(uint8_t dlc);
    static uint8_t len_to_dlc(uint8_t len);

    // 触发发送：将任意可用 TX 缓冲标记为准备好（简化为全掩码）
    bool trigger_transmit();

    // 下面为真实 CANFD 硬件操作
    // 返回可用发送缓冲区索引
    uint32_t __axiCanfdGetFreeBuffer(void);
    // 计算TRR bit位
    uint32_t __axiCanfdTrrValGetSetBitPosition(uint32_t uiValue) const;
    // CANFD字节序转换
    uint32_t __axiEndianSwap32(uint32_t uiData) const;
    // 中断使能
    void __axiCanfdInterruptEnable(uint32_t uiMask);
    // 设备滤波使能
    void __axiCanfdAcceptFilterEnable(uint32_t uiFilterIndexMask);
    // 滤波设置
    int __axiCanfdAcceptFilterSet(uint32_t uiFilterIndex, uint32_t uiMaskValue, uint32_t uiIdValue);
    // 设备滤波禁用
    void __axiCanfdAcceptFilterDisable(uint32_t uiFilterIndexMask);
    // 设备滤波设置
    uint32_t __axiCanfdSetFilter(uint32_t uiFilterIndex, uint32_t uiMask, uint32_t uiId);
    // 获取CANFD模式
    uint8_t __axiCanfdGetMode(void);
    // CANFD位时间设置
    int __axiCanfdSetBitTiming(uint8_t ucSyncJumpWidth, uint8_t ucTimeSegment2, uint16_t ucTimeSegment1);
    // 设置仲裁域波特率
    int __axiCanfdSetBaudRatePrescaler(uint8_t ucPrescaler);
    // 设置数据域位时间
    int __axiCanfdSetFBitTiming(uint8_t ucSyncJumpWidth, uint8_t ucTimeSegment2, uint8_t ucTimeSegment1);
    // 设置数据域波特率
    int __axiCanfdSetFBaudRatePrescaler(uint8_t ucPrescaler);
    // CANFD模式切换
    int __axiCanfdEnterMode(uint8_t ucMode);
    // 禁用正常模式下的波特率切换功能
    void __axiCanfdSetBitRateSwitchDisableNominal(void);
    // CANFD硬件初始化
    int __axiCanfdHwInit(void);
    // CANFD发送
    int __axiCanfdSend(CanFrame *pCanFrame);
    // CANFD接收（FIFO模式）
    int __axiCanfdRecvFifo(CanFrame *pCanFrame);
    // CANFD控制函数
    int __axiCanfdIoctl(int iCmd, void* lArg);
    // 处理CANFD总线离线事件
    void __axiCanfdPeeBusOffHandler();
    // CANFD中断处理
    int __axiCanfdIrqHandel();
};

} // namespace Device
} // namespace PhysicalLayer
} // namespace MB_DDF