/**
 * @file DDSCore.h
 * @brief DDSCore主接口类定义
 * @date 2025-08-03
 * @author Jiangkai
 * 
 * 提供DDSCore（数据分发服务）的主要接口，支持发布者-订阅者模式的消息传递。
 * 基于共享内存和无锁环形缓冲区实现高性能的进程间通信。
 */

#pragma once

#include "MB_DDF/DDS/TopicRegistry.h" // Topic注册表管理
#include "MB_DDF/DDS/SharedMemory.h" // 共享内存管理器
#include "MB_DDF/DDS/RingBuffer.h" // 环形缓冲区
#include "MB_DDF/DDS/Publisher.h" // 发布者
#include "MB_DDF/DDS/Subscriber.h" // 订阅者

#include <string>
#include <memory>
#include <unordered_map>

namespace MB_DDF {
namespace DDS {

/**
 * @brief 数据写入接口，对应Publisher
 */
using DataWriter = Publisher;

/**
 * @brief 数据读取接口，对应Subscriber
 */
using DataReader = Subscriber;

/**
 * @class DDSCore
 * @brief DDSCore主控制类，采用单例模式
 * 
 * 负责管理整个DDS系统的生命周期，包括共享内存管理、Topic注册、
 * 发布者和订阅者的创建等核心功能。
 */
class DDSCore {
public:
    /**
     * @brief 获取DDSCore单例实例
     * @return DDSCore单例引用
     */
    static DDSCore& instance();

    // 版本号，用于共享内存布局升级
    static const uint32_t VERSION = 0x00004006;
    
    /**
     * @brief 创建指定Topic的发布者
     * @param topic_name Topic名称
     * @param enable_checksum 是否启用校验和，默认true
     * @return 发布者智能指针，失败时返回nullptr
     */
    std::shared_ptr<Publisher> create_publisher(const std::string& topic_name, bool enable_checksum = true);

    /**
     * @brief 创建指定Topic的发布者，并绑定DDS句柄
     * @param topic_name Topic名称
     * @param handle 外部发布者句柄
     * @return 发布者智能指针，失败时返回nullptr
     */
    std::shared_ptr<Publisher> create_publisher(const std::string& topic_name, std::shared_ptr<Handle> handle);

    /**
     * @brief 创建指定Topic的发布者（别名）
     * @param topic_name Topic名称
     * @param enable_checksum 是否启用校验和，默认true
     * @return 发布者智能指针，失败时返回nullptr
     */
    std::shared_ptr<Publisher> create_writer(const std::string& topic_name, bool enable_checksum = true);

    /**
     * @brief 创建指定Topic的发布者（别名），并绑定DDS句柄
     * @param topic_name Topic名称
     * @param handle 外部发布者句柄
     * @return 发布者智能指针，失败时返回nullptr
     */
    std::shared_ptr<Publisher> create_writer(const std::string& topic_name, std::shared_ptr<Handle> handle);
    
    /**
     * @brief 创建指定Topic的订阅者
     * @param topic_name Topic名称
     * @param enable_checksum 是否启用校验和，默认true
     * @param callback 消息接收回调函数，默认空函数
     * @return 订阅者智能指针，失败时返回nullptr
     */
    std::shared_ptr<Subscriber> create_subscriber(const std::string& topic_name, bool enable_checksum = true, const MessageCallback& callback = nullptr);

    /**
     * @brief 创建指定Topic的订阅者，并绑定DDS句柄
     * @param topic_name Topic名称
     * @param handle 外部订阅者句柄
     * @param callback 消息接收回调函数，默认空函数
     * @return 订阅者智能指针，失败时返回nullptr
     */
    std::shared_ptr<Subscriber> create_subscriber(const std::string& topic_name, std::shared_ptr<Handle> handle, const MessageCallback& callback = nullptr);

    /**
     * @brief 创建指定Topic的订阅者（别名）
     * @param topic_name Topic名称
     * @param enable_checksum 是否启用校验和，默认true
     * @param callback 消息接收回调函数，默认空函数
     * @return 订阅者智能指针，失败时返回nullptr
     */
    std::shared_ptr<Subscriber> create_reader(const std::string& topic_name, bool enable_checksum = true, const MessageCallback& callback = nullptr);

    /**
     * @brief 创建指定Topic的订阅者（别名），并绑定DDS句柄
     * @param topic_name Topic名称
     * @param handle 外部订阅者句柄
     * @param callback 消息接收回调函数，默认空函数
     * @return 订阅者智能指针，失败时返回nullptr
     */
    std::shared_ptr<Subscriber> create_reader(const std::string& topic_name, std::shared_ptr<Handle> handle, const MessageCallback& callback = nullptr);

    /**
     * @brief 发布数据到指定Topic
     * @param publisher 发布者智能指针
     * @param data 数据指针
     * @param size 数据大小（字节）
     * @return 实际写入的数据大小（字节），0表示写入失败
     */
    size_t data_write(std::shared_ptr<Publisher> publisher, const void* data, size_t size);

    /**
     * @brief 从指定Topic的环形缓冲区读取数据
     * @param subscriber 订阅者智能指针
     * @param data 接收数据指针
     * @param size 接收数据大小（字节）
     * @return 实际读取的数据大小（字节），0表示无数据可读
     */
    size_t data_read(std::shared_ptr<Subscriber> subscriber, void* data, size_t size);

    /**
     * @brief 初始化DDS系统
     * @param shared_memory_size 共享内存大小，默认128MB
     * @return 初始化成功返回true，失败返回false
     */
    bool initialize(size_t shared_memory_size = 128 * 1024 * 1024);
    
    /**
     * @brief 关闭DDS系统，清理所有资源
     */
    void shutdown();

private:
    DDSCore() = default;
    ~DDSCore() = default;
    
    // 禁止拷贝和赋值
    DDSCore(const DDSCore&) = delete;
    DDSCore& operator=(const DDSCore&) = delete;

    std::string process_name_;                                  ///< 进程名称
    std::unique_ptr<SharedMemoryManager> shm_manager_;          ///< 共享内存管理器
    std::unique_ptr<TopicRegistry> topic_registry_;             ///< Topic注册表管理器
    std::unordered_map<TopicMetadata*, std::unique_ptr<RingBuffer>> topic_buffers_; ///< TopicMetadata指针到RingBuffer指针的映射
    // std::mutex topic_buffers_mutex_;                            ///< 保护topic_buffers_的互斥锁
    bool initialized_;                                          ///< 初始化状态标志
    
    /**
     * @brief 获取或创建指定Topic的环形缓冲区
     * @param topic_name Topic名称
     * @param enable_checksum 是否启用校验和
     * @return 环形缓冲区指针，失败时返回nullptr
     */
    RingBuffer* create_or_get_topic_buffer(const std::string& topic_name, bool enable_checksum);
    
    /**
     * @brief 通过Topic名称查找topic_buffers_中存在的TopicMetadata
     * @param topic_name Topic名称
     * @return TopicMetadata指针，未找到时返回nullptr
     */
    TopicMetadata* find_topic(const std::string& topic_name);
    
    /**
     * @brief 获取当前进程名称
     * @return 进程名称字符串
     */
    std::string get_process_name();
};

struct PubAndSub {
    std::shared_ptr<Publisher> publisher;
    std::shared_ptr<Subscriber> subscriber;
    bool write(const void* data, size_t size) { return publisher->publish(data, size); }
    size_t read(void* data, size_t size) { return subscriber->read(data, size); }
};

} // namespace DDS
} // namespace MB_DDF


