/**
 * @file MultiDeviceEpoll.h
 * @brief 支持同时 epoll 多个 XDMA 事件设备的工具类
 */
#pragma once

#include "MB_DDF/PhysicalLayer/Transport/ITransport.h"
#include <vector>
#include <unordered_map>
#include <sys/epoll.h>
#include <cstdint>

namespace MB_DDF {
namespace PhysicalLayer {
namespace Transport {

class MultiDeviceEpoll {
public:
    MultiDeviceEpoll();
    ~MultiDeviceEpoll();

    // 添加一个设备的事件 fd，返回索引（>=0），失败返回 -1
    int addDevice(const ITransport& dev);

    // 移除某设备（通过索引或 fd）。返回是否成功
    bool removeDeviceByIndex(int idx);
    bool removeDeviceByFd(int fd);

    // 等待事件。返回值：>0 表示设备索引+1，0 表示超时，<0 表示错误
    int wait(int timeout_ms);

    // 等待事件并返回状态。返回值同上，status_out 保存设备事件状态
    int waitWithStatus(int timeout_ms, uint32_t& status_out);

    // 返回当前监控的设备数量
    int size() const { return static_cast<int>(fds_.size()); }

private:
    int epfd_{-1};
    std::vector<int> fds_{};                     // 索引 -> fd
    std::unordered_map<int,int> fd_to_index_{};  // fd -> 索引
};

} // namespace Transport
} // namespace PhysicalLayer
} // namespace MB_DDF