/**
 * @file ILink.h
 * @brief 数据面抽象接口：统一帧/消息收发（阶段 A）
 * @date 2025-10-24
 * 
 * 设计要点：
 * - 不保留 ReceiveCallback；充分利用 Linux 设备即文件模型。
 * - 以 fd 暴露事件与 I/O 能力，便于与 epoll/ppoll/select 集成。
 * - 提供阻塞/非阻塞接收，返回统一语义的字节数/错误码约定。
 */
#pragma once

#include "MB_DDF/PhysicalLayer/Types.h"
#include <cstdint>
#include <cstddef>

namespace MB_DDF {
namespace PhysicalLayer {
namespace DataPlane {

class ILink {
public:
    virtual ~ILink() = default;

    // 生命周期
    virtual bool open(const LinkConfig& cfg) = 0;
    virtual bool close() = 0;

    // 发送与接收
    virtual bool     send(const uint8_t* data, uint32_t len) = 0;
    virtual int32_t  receive(uint8_t* buf, uint32_t buf_size) = 0;                // 非阻塞：>0字节；0无数据；<0错误
    virtual int32_t  receive(uint8_t* buf, uint32_t buf_size, uint32_t timeout_us) = 0; // 阻塞带超时

    // 状态与属性
    virtual LinkStatus getStatus() const = 0;
    virtual uint16_t   getMTU() const = 0;

    // 事件与 I/O：返回可用于 epoll 的 fd；<0 表示不支持 fd 模型
    virtual int getEventFd() const { return -1; }

    // 可选：对于文件型 I/O 的直接读写 fd（如 socket/设备管道），<0 表示无
    virtual int getIOFd() const { return -1; }

    // 通用设备控制接口：类似 ioctl，opcode 表示操作码，in/out 为可选结构体指针
    // 约定：
    // - 返回值 >=0 表示成功，可为写入/读取的字节数或具体结果码；
    // - 返回值 <0 表示错误（如不支持、参数错误、设备错误等）。
    virtual int ioctl(uint32_t opcode, const void* in, size_t in_len, void* out = nullptr, size_t out_len = 0) = 0;
};

} // namespace DataPlane
} // namespace PhysicalLayer
} // namespace MB_DDF