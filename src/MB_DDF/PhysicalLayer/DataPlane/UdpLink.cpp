/**
 * @file UdpLink.cpp
 */
#include "MB_DDF/PhysicalLayer/DataPlane/UdpLink.h"
#include "MB_DDF/PhysicalLayer/Support/Log.h"
#include <cstring>
#include <poll.h>

namespace MB_DDF {
namespace PhysicalLayer {
namespace DataPlane {

namespace {
static int set_nonblock(int fd) {
    int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0) return -1;
    if (::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) return -1;
    return 0;
}
}

UdpLink::UdpLink() = default;
UdpLink::~UdpLink() { close(); }

bool UdpLink::makeSockAddrIPv4(const std::string& ip, uint16_t port, SockAddr& out) {
    sockaddr_in sin{};
    sin.sin_family = AF_INET;
    sin.sin_port   = htons(port);
    if (::inet_pton(AF_INET, ip.c_str(), &sin.sin_addr) != 1) return false;
    std::memcpy(&out.ss, &sin, sizeof(sin));
    out.len = sizeof(sin);
    return true;
}

bool UdpLink::parseConfigName(const std::string& name, std::optional<SockAddr>& local, std::optional<SockAddr>& remote) {
    // 格式1："<local_port>"
    // 格式2："<local_ip>:<local_port>|<remote_ip>:<remote_port>"
    if (name.empty()) return false;
    auto pos = name.find('|');
    if (pos == std::string::npos) {
        uint16_t port = static_cast<uint16_t>(std::stoi(name));
        SockAddr sa{};
        if (!makeSockAddrIPv4("0.0.0.0", port, sa)) return false;
        local = sa;
        remote.reset();
        return true;
    } else {
        auto left = name.substr(0, pos);
        auto right = name.substr(pos + 1);
        auto colonL = left.find(':');
        auto colonR = right.find(':');
        if (colonL == std::string::npos || colonR == std::string::npos) return false;
        auto lip = left.substr(0, colonL);
        auto lport = static_cast<uint16_t>(std::stoi(left.substr(colonL + 1)));
        auto rip = right.substr(0, colonR);
        auto rport = static_cast<uint16_t>(std::stoi(right.substr(colonR + 1)));
        SockAddr la{}, ra{};
        if (!makeSockAddrIPv4(lip, lport, la)) return false;
        if (!makeSockAddrIPv4(rip, rport, ra)) return false;
        local = la;
        remote = ra;
        return true;
    }
}

bool UdpLink::open(const LinkConfig& cfg) {
    if (sock_fd_ >= 0) return true;
    cfg_ = cfg;
    if (!parseConfigName(cfg_.name, local_, remote_)) return false;

    int fd = ::socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        LOGE("udp", "socket", errno, "name=%s", cfg_.name.c_str());
        return false;
    }

    // 绑定本地地址
    if (local_.has_value()) {
        if (::bind(fd, reinterpret_cast<const sockaddr*>(&local_->ss), local_->len) < 0) {
            LOGE("udp", "bind", errno, "local_port_or_addr=%s", cfg_.name.c_str());
            ::close(fd);
            return false;
        }
    }

    // 非阻塞模式
    if (set_nonblock(fd) < 0) {
        LOGE("udp", "nonblock", errno, "");
        ::close(fd);
        return false;
    }

    sock_fd_ = fd;
    status_ = LinkStatus::OPEN;
    LOGI("udp", "open", 0, "fd=%d name=%s", sock_fd_, cfg_.name.c_str());
    return true;
}

bool UdpLink::close() {
    if (sock_fd_ >= 0) {
        ::close(sock_fd_);
        sock_fd_ = -1;
    }
    status_ = LinkStatus::CLOSED;
    LOGI("udp", "close", 0, "done");
    return true;
}

bool UdpLink::send(const uint8_t* data, uint32_t len, const Endpoint& dst) {
    if (sock_fd_ < 0 || status_ != LinkStatus::OPEN) return false;
    const sockaddr* to = nullptr;
    socklen_t tolen = 0;
    SockAddr tmp{};
    if (remote_.has_value()) {
        to = reinterpret_cast<const sockaddr*>(&remote_->ss);
        tolen = remote_->len;
    } else {
        // 若未指定远端，将 channel_id 作为端口，发送到本机
        if (dst.channel_id <= 0) return false;
        if (!makeSockAddrIPv4("127.0.0.1", static_cast<uint16_t>(dst.channel_id), tmp)) return false;
        to = reinterpret_cast<const sockaddr*>(&tmp.ss);
        tolen = tmp.len;
    }
    ssize_t n = ::sendto(sock_fd_, data, len, 0, to, tolen);
    if (n != static_cast<ssize_t>(len)) {
        LOGE("udp", "sendto", errno, "fd=%d", sock_fd_);
        return false;
    }
    return true;
}

int32_t UdpLink::receive(uint8_t* buf, uint32_t buf_size, Endpoint& src) {
    // 仅在已打开状态下才允许接收；返回负数表示错误
    if (sock_fd_ < 0 || status_ != LinkStatus::OPEN) return -1;
    sockaddr_storage from{};
    socklen_t fromlen = sizeof(from);
    ssize_t n = ::recvfrom(sock_fd_, buf, buf_size, MSG_DONTWAIT, reinterpret_cast<sockaddr*>(&from), &fromlen);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
        LOGE("udp", "recvfrom", errno, "fd=%d", sock_fd_);
        return -1;
    }
    // 提取源端口到 Endpoint.channel_id（IPv4）
    if (from.ss_family == AF_INET) {
        auto* sin = reinterpret_cast<sockaddr_in*>(&from);
        src.channel_id = ntohs(sin->sin_port);
    } else {
        src.channel_id = -1;
    }
    // 更新默认远端地址为最近的来源地址（learn remote）
    SockAddr sa{};
    sa.ss = from;
    sa.len = fromlen;
    remote_ = sa;
    return static_cast<int32_t>(n);
}

int32_t UdpLink::receive(uint8_t* buf, uint32_t buf_size, Endpoint& src, uint32_t timeout_us) {
    if (sock_fd_ < 0 || status_ != LinkStatus::OPEN) return -1;
    struct pollfd pfd{ sock_fd_, POLLIN, 0 };
    int timeout_ms = static_cast<int>(timeout_us / 1000);
    int ret = ::poll(&pfd, 1, timeout_ms);
    if (ret == 0) return 0;       // 超时
    if (ret < 0) {
        LOGE("udp", "poll", errno, "fd=%d", sock_fd_);
        return -1;       // 错误
    }
    if (pfd.revents & POLLIN) {
        return receive(buf, buf_size, src);
    }
    return 0;
}

LinkStatus UdpLink::getStatus() const { return status_; }
uint16_t   UdpLink::getMTU() const { return cfg_.mtu; }
int        UdpLink::getEventFd() const { return sock_fd_; }
int        UdpLink::getIOFd() const { return sock_fd_; }

int UdpLink::ioctl(uint32_t opcode, const void* in, size_t in_len, void* out, size_t out_len) {
    (void)opcode; (void)in; (void)in_len; (void)out; (void)out_len;
    return -ENOTSUP; // UDP 链路默认不支持设备特定 ioctl
}

} // namespace DataPlane
} // namespace PhysicalLayer
} // namespace MB_DDF