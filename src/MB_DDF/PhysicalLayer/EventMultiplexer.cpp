/**
 * @file EventMultiplexer.cpp
 */
#include "MB_DDF/PhysicalLayer/EventMultiplexer.h"
#include <cerrno>

namespace MB_DDF {
namespace PhysicalLayer {

EventMultiplexer::EventMultiplexer() {
    epfd_ = ::epoll_create1(EPOLL_CLOEXEC);
}

EventMultiplexer::~EventMultiplexer() {
    if (epfd_ >= 0) {
        ::close(epfd_);
        epfd_ = -1;
    }
}

bool EventMultiplexer::add(int fd, uint32_t events, Callback cb) {
    if (fd < 0 || epfd_ < 0) return false;
    epoll_event ev{};
    ev.events = events;
    ev.data.fd = fd;
    int op = cbs_.count(fd) ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;
    if (::epoll_ctl(epfd_, op, fd, &ev) < 0) return false;
    cbs_[fd] = std::move(cb);
    return true;
}

bool EventMultiplexer::remove(int fd) {
    if (fd < 0 || epfd_ < 0) return false;
    ::epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr);
    cbs_.erase(fd);
    return true;
}

int EventMultiplexer::wait_once(int timeout_ms) {
    if (epfd_ < 0) return -1;
    epoll_event evs[32];
    int n = ::epoll_wait(epfd_, evs, 32, timeout_ms);
    if (n <= 0) return n; // 0 超时；<0 错误
    for (int i = 0; i < n; ++i) {
        int fd = evs[i].data.fd;
        auto it = cbs_.find(fd);
        if (it != cbs_.end() && it->second) {
            it->second(fd, evs[i].events);
        }
    }
    return n;
}

void EventMultiplexer::run_loop(int timeout_ms) {
    running_ = true;
    while (running_) {
        int r = wait_once(timeout_ms);
        if (r < 0 && errno == EINTR) continue; // 中断重试
        // 其他错误/超时无需特殊处理，继续循环
    }
}

void EventMultiplexer::stop() { running_ = false; }

} // namespace PhysicalLayer
} // namespace MB_DDF