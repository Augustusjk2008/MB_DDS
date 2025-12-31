/**
 * @file EventMultiplexer.h
 * @brief 统一事件多路复用器：注册 ILink/IDeviceTransport 的 fd 并分发
 * @date 2025-10-24
 *
 * 设计要点：
 * - 统一接入 Linux epoll，注册来自数据面/控制面的事件 fd。
 * - 提供回调签名与分发机制，不持有对象所有权，仅保存 fd 与回调。
 * - 支持添加/移除、单次等待与循环驱动。
 */
#pragma once

#include <sys/epoll.h>
#include <unistd.h>
#include <functional>
#include <unordered_map>

namespace MB_DDF {
namespace PhysicalLayer {

// 用法示例：
//   EventMultiplexer mux;
//   mux.add(link.getEventFd(), EPOLLIN, [&](int fd, uint32_t ev){ /* 拉取数据并处理 */ });
//   mux.add(tp.getEventFd(), EPOLLIN, [&](int fd, uint32_t ev){ /* 读取事件并触发收取 */ });
//   mux.run_loop(100); // 100ms 超时循环
class EventMultiplexer {
public:
    using Callback = std::function<void(int fd, uint32_t events)>; // events: EPOLL* flags

    EventMultiplexer();
    ~EventMultiplexer();

    // 注册一个 fd 与其事件掩码与回调；重复注册将更新掩码与回调
    bool add(int fd, uint32_t events, Callback cb);
    // 从 epoll 中移除一个 fd，并删除回调
    bool remove(int fd);

    // 阻塞等待一次事件并分发；返回触发事件数量（>=0）
    int wait_once(int timeout_ms);

    // 循环运行，直到 stop() 被调用；每次 wait_once 以 timeout_ms 为超时
    void run_loop(int timeout_ms);
    void stop();

    int epoll_fd() const { return epfd_; }

private:
    int epfd_{-1};
    bool running_{false};
    std::unordered_map<int, Callback> cbs_{};
};

} // namespace PhysicalLayer
} // namespace MB_DDF