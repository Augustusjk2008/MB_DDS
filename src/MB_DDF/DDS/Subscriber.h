/**
 * @file Subscriber.h
 * @brief 订阅者类定义
 * @date 2025-08-03
 * @author Jiangkai
 * 
 * 提供消息订阅功能，支持从指定Topic的环形缓冲区中接收消息。
 * 采用异步回调机制，在独立线程中处理消息接收和分发。
 */

#pragma once

#include "MB_DDF/DDS/RingBuffer.h"
#include "MB_DDF/DDS/TopicRegistry.h"
#include "MB_DDF/DDS/DDSHandle.h"
#include <cstddef>
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <cstdint>
#include <vector>

namespace MB_DDF {
namespace DDS {

/**
 * @typedef MessageCallback
 * @brief 消息回调函数类型定义
 * @param data 消息数据指针
 * @param size 消息数据大小
 * @param timestamp 消息时间戳（纳秒）
 */
using MessageCallback = std::function<void(const void* data, size_t size, uint64_t timestamp)>;

/**
 * @class Subscriber
 * @brief 消息订阅者类
 * 
 * 负责从指定Topic的环形缓冲区中接收消息，并通过回调函数异步通知用户。
 * 使用独立的工作线程进行消息轮询，避免阻塞主线程。
 */
class Subscriber {
public:
    /**
     * @brief 构造函数
     * @param metadata Topic元数据指针
     * @param ring_buffer 关联的环形缓冲区指针
     * @param subscriber_name 订阅者名称（可选，默认为空）
     * @param handle 外部接收者句柄（可选，默认为空）
     */
    Subscriber(TopicMetadata* metadata, RingBuffer* ring_buffer, const std::string& subscriber_name = "", std::shared_ptr<Handle> handle = nullptr);
    
    /**
     * @brief 析构函数，自动取消订阅并清理资源
     */
    ~Subscriber();

    /**
     * @brief 开始订阅消息
     * @param callback 消息接收回调函数
     * @return 订阅成功返回true，失败返回false
     */
    bool subscribe(MessageCallback callback = nullptr);
    
    /**
     * @brief 取消订阅，停止接收消息
     */
    void unsubscribe();
    
    /**
     * @brief 检查是否正在订阅
     * @return 正在订阅返回true，否则返回false
     */
    bool is_subscribed() const { return subscribed_.load(); }

    /**
     * @brief 绑定订阅者工作线程到指定CPU核心
     * @param cpu_id CPU核心ID（从0开始）
     * @return 绑定成功返回true，失败返回false
     */
    bool bind_to_cpu(int cpu_id);

    /**
     * @brief 从环形缓冲区读取消息
     * @param data 接收消息数据的指针
     * @param size 接收消息数据的最大大小
     * @param latest 是否读取最新消息（默认true）
     * @return 实际读取的消息大小（0表示无消息）
     */
    size_t read(void* data, size_t size, bool latest = true);

    /**
     * @brief 获取订阅者工作线程
     * @return 工作线程对象引用
     */
    std::thread* get_thread() { 
        if (callback_) {
            return &worker_thread_; 
        } else {
            return nullptr;
        }
    }

private:
    TopicMetadata* metadata_;       ///< Topic元数据指针
    RingBuffer* ring_buffer_;       ///< 环形缓冲区指针
    MessageCallback callback_;      ///< 消息回调函数
    std::atomic<bool> subscribed_;  ///< 订阅状态标志
    std::atomic<bool> running_;     ///< 工作线程运行状态标志
    std::thread worker_thread_;     ///< 消息接收工作线程
    std::shared_ptr<Handle> handle_; ///< 外部接收者句柄
    std::vector<uint8_t> receive_buffer_{}; ///< 接收消息缓存

    // 自身信息
    uint64_t subscriber_id_;        ///< 唯一的订阅者ID
    std::string subscriber_name_;   ///< 订阅者名称
    SubscriberState* subscriber_state_; ///< 订阅者状态结构体指针

    /**
     * @brief 工作线程主循环函数
     * 持续从环形缓冲区中读取消息并调用回调函数
     */
    void worker_loop();

    /**
     * @brief 从环形缓冲区读取下一条消息
     * @param data 接收消息数据的指针
     * @param size 接收消息数据的最大大小
     * @return 实际读取的消息大小（0表示无消息）
     */
    size_t read_next(void* data, size_t size);

    /**
     * @brief 从环形缓冲区读取最新消息
     * @param data 接收消息数据的指针
     * @param size 接收消息数据的最大大小
     * @return 实际读取的消息大小（0表示无消息）
     */
    size_t read_latest(void* data, size_t size);
};

} // namespace DDS
} // namespace MB_DDF

