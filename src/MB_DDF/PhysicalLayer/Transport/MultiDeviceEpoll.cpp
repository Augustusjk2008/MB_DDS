#include "MB_DDF/PhysicalLayer/Transport/MultiDeviceEpoll.h"
#include <unistd.h>
#include <cerrno>

namespace MB_DDF {
namespace PhysicalLayer {
namespace Transport {

MultiDeviceEpoll::MultiDeviceEpoll() {
    epfd_ = ::epoll_create1(EPOLL_CLOEXEC);
}

MultiDeviceEpoll::~MultiDeviceEpoll() {
    if (epfd_ >= 0) {
        ::close(epfd_);
        epfd_ = -1;
    }
}

int MultiDeviceEpoll::addDevice(const ITransport& dev) {
    // 从 ITransport 获取事件 fd（若不支持则返回 -1）
    int fd = dev.getEventFd();
    if (fd < 0) return -1;
    if (fd_to_index_.count(fd)) return fd_to_index_[fd]; // 已存在则返回原索引

    // 加入 epoll 监听
    struct epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = fd; // 使用 fd 存储在 data
    if (::epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &ev) < 0) {
        return -1;
    }

    // 维护索引映射
    int idx = static_cast<int>(fds_.size());
    fds_.push_back(fd);
    fd_to_index_[fd] = idx;
    return idx;
}

bool MultiDeviceEpoll::removeDeviceByIndex(int idx) {
    if (idx < 0 || idx >= static_cast<int>(fds_.size())) return false;
    int fd = fds_[idx];
    if (fd < 0) return false;
    if (::epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr) < 0) {
        return false;
    }
    fd_to_index_.erase(fd);
    fds_[idx] = -1; // 保留槽位，但标记无效
    return true;
}

bool MultiDeviceEpoll::removeDeviceByFd(int fd) {
    auto it = fd_to_index_.find(fd);
    if (it == fd_to_index_.end()) return false;
    return removeDeviceByIndex(it->second);
}

int MultiDeviceEpoll::wait(int timeout_ms) {
    if (epfd_ < 0) return -1;
    struct epoll_event ev;
    int rc = ::epoll_wait(epfd_, &ev, 1, timeout_ms);
    if (rc == 0) return 0; // 超时
    if (rc < 0) return -1; // 错误

    int fd = ev.data.fd;
    auto it = fd_to_index_.find(fd);
    if (it == fd_to_index_.end()) return -1;
    return it->second + 1; // 事件索引从 1 开始返回，避免与 0 超时歧义
}

int MultiDeviceEpoll::waitWithStatus(int timeout_ms, uint32_t& status_out) {
    status_out = 0;
    int rc = wait(timeout_ms);
    if (rc <= 0) {
        return rc; // 超时或错误原样返回
    }
    int idx = rc - 1;
    if (idx < 0 || idx >= static_cast<int>(fds_.size())) { 
        return -1;
    }
    int fd = fds_[idx];
    if (fd < 0) { 
        return -1;
    }
    ssize_t n = ::read(fd, &status_out, sizeof(status_out));
    if (n != static_cast<ssize_t>(sizeof(status_out))) { 
        return -1;
    }
    return rc;
}

} // namespace Transport
} // namespace PhysicalLayer
} // namespace MB_DDF