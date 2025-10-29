/**
 * @file UdpLink.h
 * @brief 基于 UDP 的数据面实现，适配 ILink（阶段 B）
 * @date 2025-10-24
 * 
 * 说明：
 * - 去除回调，暴露 socket fd 以融入 epoll。
 * - LinkConfig.name 支持简单的连接参数：
 *   格式1："<local_port>" （仅绑定本地端口，不指定远端）
 *   格式2："<local_ip>:<local_port>|<remote_ip>:<remote_port>" （绑定并连接到远端）
 * - Endpoint.channel_id 在 UDP 场景下用于标识源/目的端口（可选）。
 */
#pragma once

#include "MB_DDF/PhysicalLayer/DataPlane/ILink.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <string>
#include <optional>

namespace MB_DDF {
namespace PhysicalLayer {
namespace DataPlane {

class UdpLink : public ILink {
public:
    UdpLink();
    ~UdpLink() override;

    bool open(const LinkConfig& cfg) override;
    bool close() override;

    bool    send(const uint8_t* data, uint32_t len) override;
    int32_t receive(uint8_t* buf, uint32_t buf_size) override;
    int32_t receive(uint8_t* buf, uint32_t buf_size, uint32_t timeout_us) override;

    LinkStatus getStatus() const override;
    uint16_t   getMTU() const override;

    int getEventFd() const override;
    int getIOFd() const override;

    // 对 UDP 链路的 ioctl 默认不支持，返回 -ENOTSUP
    int ioctl(uint32_t opcode, const void* in, size_t in_len, void* out, size_t out_len) override;

private:
    struct SockAddr {
        sockaddr_storage ss{};
        socklen_t        len{0};
    };

    // 解析 LinkConfig.name 为本地/远端地址：
    // - "<local_port>" 仅绑定本地端口；
    // - "<local_ip>:<local_port>|<remote_ip>:<remote_port>" 同时绑定并指定默认远端。
    static bool parseConfigName(const std::string& name, std::optional<SockAddr>& local, std::optional<SockAddr>& remote);
    static bool makeSockAddrIPv4(const std::string& ip, uint16_t port, SockAddr& out);

    int         sock_fd_{-1};
    LinkStatus  status_{LinkStatus::CLOSED};
    LinkConfig  cfg_{};
    std::optional<SockAddr> local_{};
    std::optional<SockAddr> remote_{};
};

} // namespace DataPlane
} // namespace PhysicalLayer
} // namespace MB_DDF