/**
 * @file RingBuffer.h
 * @brief 无锁环形缓冲区实现
 * @date 2025-08-03
 * @author Jiangkai
 * 
 * 提供高性能的无锁环形缓冲区实现，支持单生产者多消费者模式。
 * 基于原子操作和内存屏障确保多进程安全，适用于高频消息传递场景。
 * 直接集成Message结构，避免序列号重复，支持消息损坏检测。
 */

#pragma once

#include "Message.h"
#include <atomic>
#include <cstdint>
#include <cstring>
#include <string>
#include <semaphore.h>
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <vector>

namespace MB_DDF {
namespace DDS {

/**
 * @struct SubscriberState
 * @brief 订阅者状态结构，存储在共享内存中
 * 
 * 每个订阅者维护自己的读取进度，支持多进程安全访问。
 */
struct alignas(64) SubscriberState {
    std::atomic<uint64_t> read_pos;              ///< 当前读取位置（字节偏移）
    std::atomic<uint64_t> last_read_sequence;    ///< 最后成功读取的消息序列号
    std::atomic<uint64_t> timestamp;             ///< 最后读取消息时间戳（纳秒精度）
    uint64_t subscriber_id;                      ///< 订阅者唯一标识符
    char subscriber_name[64];                    ///< 订阅者名称（最大63字符 + 终止符）
    
    SubscriberState() : read_pos(0), last_read_sequence(0), 
                       timestamp(0), subscriber_id(0) {
        subscriber_name[0] = '\0';
    }
};


/**
 * @struct Header
 * @brief 环形缓冲区头部结构，存储在共享内存中
 */
struct alignas(64) RingHeader {
    std::atomic<uint64_t> write_pos;          ///< 当前写入位置（字节偏移）
    std::atomic<uint64_t> current_sequence;   ///< 当前消息序列号
    std::atomic<uint32_t> notification_count; ///< futex通知计数
    std::atomic<uint64_t> timestamp;          ///< 最新消息时间戳（纳秒精度）
    size_t capacity;                          ///< 数据区容量
    size_t data_offset;                       ///< 数据区起始偏移
    uint32_t magic_number;                    ///< 魔数，用于验证初始化
    uint64_t publisher_id;                    ///< 发布者唯一标识符
    char publisher_name[64];                  ///< 发布者名称（最大63字符 + 终止符）
        
    static constexpr uint32_t MAGIC = 0x52494E47; // "RING"
        
    RingHeader() : write_pos(0), current_sequence(0), notification_count(0),
              capacity(0), data_offset(0), magic_number(MAGIC), publisher_id(0) {
        publisher_name[0] = '\0';
    }
};

/**
 * @class RingBuffer
 * @brief 无锁环形缓冲区类，支持单生产者多消费者模式
 * 
 * 基于共享内存的环形缓冲区，直接存储Message结构，避免序列号重复。
 * 支持多进程安全访问，订阅者自管理读取进度，提供消息损坏检测功能。
 */
class RingBuffer {
public:
    /**
     * @brief 构造函数
     * @param buffer 缓冲区内存地址（由TopicRegistry分配）
     * @param size 缓冲区总大小
     * @param sem 共享内存信号量（用于发布者和订阅者注册保护）
     */
    RingBuffer(void* buffer, size_t size, sem_t* sem);
    
    /**
     * @brief 发布消息到环形缓冲区
     * @param data 要发布的消息数据指针
     * @param size 消息数据大小（字节数）
     * @return 发布成功返回true，缓冲区满时返回false
     */
    bool publish_message(const void* data, size_t size);
    
    /**
     * @brief 订阅者读取下一条消息
     * @param subscriber 输入/输出参数，订阅者状态结构体，包含读取位置和最后读取序列号
     * @param out_message 输出参数，返回读取到的消息
     * @param next_expected_sequence 输入参数，期望的下一条消息序列号
     * @return 读取成功返回true，无新消息返回false
     */
    bool read_expected(SubscriberState* subscriber, Message*& out_message, uint64_t next_expected_sequence);
    
    /**
     * @brief 订阅者读取下一条消息
     * @param subscriber 输入/输出参数，订阅者状态结构体，包含读取位置和最后读取序列号
     * @param out_message 输出参数，返回读取到的消息
     * @return 读取成功返回true，无新消息返回false
     */
    bool read_next(SubscriberState* subscriber, Message*& out_message);
    
    /**
     * @brief 订阅者跳转到最新消息
     * @param subscriber 输入/输出参数，订阅者状态结构体，包含读取位置和最后读取序列号
     * @param out_message 输出参数，返回最新消息
     * @return 成功返回true，无消息返回false
     */
    bool read_latest(SubscriberState* subscriber, Message*& out_message);
    
    /**
     * @brief 获取订阅者未读消息数量
     * @param subscriber 输入参数，订阅者状态结构体，包含读取位置和最后读取序列号
     * @return 未读消息数量
     */
    uint64_t get_unread_count(SubscriberState* subscriber);

    /**
     * @brief 设置发布者信息
     * @param publisher_id 发布者唯一标识符
     * @param publisher_name 发布者名称
     * @return 设置成功返回true，失败返回false
     */
    bool set_publisher(uint64_t publisher_id, const std::string& publisher_name);

    /**
     * @brief 移除发布者信息
     */
    void remove_publisher();
    
    /**
     * @brief 注册新的订阅者（使用信号量保护）
     * @param subscriber_id 订阅者唯一标识符
     * @param subscriber_name 订阅者名称
     * @return 注册成功返回订阅者状态结构体指针，失败返回nullptr
     */
    SubscriberState* register_subscriber(uint64_t subscriber_id, const std::string& subscriber_name);
    
    /**
     * @brief 注销订阅者（使用信号量保护）
     * @param subscriber 输入参数，订阅者状态结构体指针
     */
    void unregister_subscriber(SubscriberState* subscriber);
    
    /**
     * @brief 等待新消息通知（基于futex）
     * @param subscriber 输入参数，订阅者状态结构体指针，包含读取位置和最后读取序列号
     * @param timeout_ms 超时时间（毫秒），0表示无限等待
     * @return 有新消息返回true，超时返回false
     */
    bool wait_for_message(SubscriberState* subscriber, uint32_t timeout_ms = 0);
    
    /**
     * @brief 检查缓冲区是否为空
     * @return 缓冲区为空返回true，否则返回false
     */
    bool empty() const;
    
    /**
     * @brief 检查缓冲区是否已满
     * @return 缓冲区已满返回true，否则返回false
     */
    bool full() const;
    
    /**
     * @brief 获取可用写入空间大小
     * @return 可用空间大小（字节）
     */
    size_t available_space() const;
    
    /**
     * @brief 获取可读取数据大小
     * @return 可读取数据大小（字节）
     */
    size_t available_data() const;
    
    /**
     * @brief 通知所有订阅者有新消息（futex唤醒）
     */
    void notify_subscribers();
    
    /**
     * @brief 获取缓冲区统计信息
     */
    struct Statistics {
        uint64_t total_messages;      ///< 总消息数
        uint64_t current_sequence;    ///< 当前序列号
        size_t available_space;       ///< 可用空间
        uint32_t active_subscribers;  ///< 活跃订阅者数
        //所有订阅者的名字和ID
        std::vector<std::pair<uint64_t, std::string>> subscribers;
    };
    
    Statistics get_statistics() const;

private:    
    /**
     * @struct SubscriberRegistry
     * @brief 订阅者注册表，存储在共享内存中
     */
    struct alignas(64) SubscriberRegistry {
        static constexpr size_t MAX_SUBSCRIBERS = 64;
        std::atomic<uint32_t> count;              ///< 当前订阅者数量
        SubscriberState subscribers[MAX_SUBSCRIBERS]; ///< 订阅者状态数组
        
        SubscriberRegistry() : count(0) {}
    };

    RingHeader* header_;                    ///< 缓冲区头部指针
    SubscriberRegistry* registry_;      ///< 订阅者注册表指针
    char* data_;                       ///< 数据存储区指针
    size_t capacity_;                  ///< 数据区容量
    sem_t* sem_;                       ///< 共享内存信号量    
    
    /**
     * @brief 检查是否可以写入指定大小的消息
     * @param message_size 消息大小
     * @return 可以写入返回true，否则返回false
     */
    bool can_write(size_t message_size) const;
    
    /**
     * @brief 查找订阅者状态
     * @param subscriber_id 订阅者ID
     * @return 订阅者状态指针，未找到返回nullptr
     */
    SubscriberState* find_subscriber(uint64_t subscriber_id) const;
    
    /**
     * @brief 在指定位置读取消息
     * @param pos 消息位置
     * @return 消息指针，失败返回nullptr
     */
    Message* read_message_at(size_t pos) const;
    
    /**
     * @brief 验证消息完整性
     * @param message 消息指针
     * @return 消息有效返回true，否则返回false
     */
    bool validate_message(const Message* message) const;
    
    /**
     * @brief 查找下一条有效消息
     * @param start_pos 开始搜索位置
     * @param out_pos 输出参数，找到的消息位置
     * @return 找到有效消息返回true，否则返回false
     */
    bool find_next_valid_message(size_t start_pos, size_t& out_pos) const;
    
    /**
     * @brief futex系统调用封装
     */
    static int futex_wait(volatile uint32_t* addr, uint32_t expected, uint32_t timeout_ms = 0);
    static int futex_wake(volatile uint32_t* addr, uint32_t count = INT32_MAX);
    
    /**
     * @brief 计算消息在缓冲区中的总大小（包含对齐）
     * @param data_size 消息数据大小
     * @return 总大小
     */
    static size_t calculate_message_total_size(size_t data_size);
    
    /**
     * @brief 内存对齐大小
     */
    static constexpr size_t ALIGNMENT = 8;
};

} // namespace DDS
} // namespace MB_DDF

