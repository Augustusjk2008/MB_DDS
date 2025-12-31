/**
 * @file DDSMonitor.h
 * @brief DDS系统监控模块
 * @date 2025-10-16
 * @author Jiangkai
 * 
 * 提供DDS系统的监控功能，定期扫描共享内存中的发布订阅者信息，
 * 收集活跃时间等统计数据，并提供序列化功能用于数据传输。
 */

#pragma once

#include "MB_DDF/DDS/DDSCore.h"
#include <cstdint>
#include <memory>
#include <vector>
#include <string>
#include <atomic>
#include <thread>
#include <functional>

namespace MB_DDF {
namespace Monitor {

// 前向声明
class SharedMemoryAccessor;

/**
 * @struct PublisherInfo
 * @brief 发布者信息结构
 */
struct PublisherInfo {
    uint64_t publisher_id;          ///< 发布者ID
    std::string publisher_name;     ///< 发布者名称
    std::string topic_name;         ///< Topic名称
    uint32_t topic_id;              ///< Topic ID
    uint64_t last_sequence;         ///< 最后发布的消息序列号
    bool is_active;                 ///< 是否活跃
    
    PublisherInfo() : publisher_id(0), topic_id(0), last_sequence(0), is_active(false) {}
};

/**
 * @struct SubscriberInfo
 * @brief 订阅者信息结构
 */
struct SubscriberInfo {
    uint64_t subscriber_id;         ///< 订阅者ID
    std::string subscriber_name;    ///< 订阅者名称
    std::string topic_name;         ///< Topic名称
    uint32_t topic_id;              ///< Topic ID
    uint64_t read_pos;              ///< 当前读取位置
    uint64_t last_read_sequence;    ///< 最后读取的消息序列号
    uint64_t last_active_time;      ///< 最后活跃时间（纳秒时间戳）
    bool is_active;                 ///< 是否活跃
    
    SubscriberInfo() : subscriber_id(0), topic_id(0), read_pos(0), 
                      last_read_sequence(0), last_active_time(0), is_active(false) {}
};

/**
 * @struct TopicInfo
 * @brief Topic信息结构
 */
struct TopicInfo {
    uint32_t topic_id;              ///< Topic ID
    std::string topic_name;         ///< Topic名称
    size_t ring_buffer_size;        ///< 环形缓冲区大小
    bool has_publisher;             ///< 是否有发布者
    uint32_t subscriber_count;      ///< 订阅者数量
    uint64_t total_messages;        ///< 总消息数
    size_t available_space;         ///< 可用空间
    
    TopicInfo() : topic_id(0), ring_buffer_size(0), has_publisher(false), 
                 subscriber_count(0), total_messages(0), available_space(0) {}
};

/**
 * @struct DDSSystemSnapshot
 * @brief DDS系统快照数据结构
 */
struct DDSSystemSnapshot {
    uint64_t timestamp;                         ///< 快照时间戳（纳秒）
    uint32_t dds_version;                       ///< DDS版本号
    std::vector<TopicInfo> topics;              ///< Topic信息列表
    std::vector<PublisherInfo> publishers;      ///< 发布者信息列表
    std::vector<SubscriberInfo> subscribers;    ///< 订阅者信息列表
    size_t total_shared_memory_size;            ///< 共享内存总大小
    size_t used_shared_memory_size;             ///< 已使用的共享内存大小
    
    DDSSystemSnapshot() : timestamp(0), total_shared_memory_size(0), used_shared_memory_size(0) {}
};

/**
 * @class DDSMonitor
 * @brief DDS系统监控器类
 * 
 * 定期扫描DDS系统的共享内存，收集发布订阅者信息和活跃状态，
 * 提供数据序列化功能用于远程监控和分析。
 */
class DDSMonitor {
public:
    /**
     * @brief 构造函数
     * @param scan_interval_ms 扫描间隔（毫秒），默认1000ms
     * @param activity_timeout_ms 活跃超时时间（毫秒），默认5000ms
     */
    explicit DDSMonitor(uint32_t scan_interval_ms = 1000, uint32_t activity_timeout_ms = 5000);
    
    /**
     * @brief 析构函数
     */
    ~DDSMonitor();

    /**
     * @brief 初始化监控器
     * @param dds_core DDS核心实例引用
     * @return 初始化成功返回true，失败返回false
     */
    bool initialize(DDS::DDSCore& dds_core);
    
    /**
     * @brief 启动监控
     * @return 启动成功返回true，失败返回false
     */
    bool start_monitoring();
    
    /**
     * @brief 停止监控
     */
    void stop_monitoring();
    
    /**
     * @brief 执行一次系统扫描
     * @return 系统快照数据
     */
    DDSSystemSnapshot scan_system();

    /**
     * @brief 将DDS版本号转换为字符串
     * @param version DDS版本号
     * @return 版本号字符串
     */
    static std::string version_to_string(uint32_t version);
    
    /**
     * @brief 序列化快照数据为JSON格式
     * @param snapshot 系统快照数据
     * @return JSON格式的字符串
     */
    std::string serialize_to_json(const DDSSystemSnapshot& snapshot);
    
    /**
     * @brief 序列化快照数据为二进制格式
     * @param snapshot 系统快照数据
     * @param buffer 输出缓冲区
     * @param buffer_size 缓冲区大小
     * @return 实际序列化的字节数，0表示失败
     */
    size_t serialize_to_binary(const DDSSystemSnapshot& snapshot, void* buffer, size_t buffer_size);
    
    /**
     * @brief 设置监控数据回调函数
     * @param callback 回调函数，参数为系统快照数据
     */
    void set_monitor_callback(std::function<void(const DDSSystemSnapshot&)> callback);
    
    /**
     * @brief 获取监控统计信息
     * @return 最新的系统快照数据
     */
    DDSSystemSnapshot get_latest_snapshot() const;
    
    /**
     * @brief 检查监控器是否正在运行
     * @return 正在运行返回true，否则返回false
     */
    bool is_monitoring() const { return monitoring_.load(); }

private:
    uint32_t scan_interval_ms_;                 ///< 扫描间隔（毫秒）
    uint32_t activity_timeout_ms_;              ///< 活跃超时时间（毫秒）
    
    DDS::DDSCore* dds_core_;                    ///< DDS核心实例指针
    std::unique_ptr<SharedMemoryAccessor> shm_accessor_; ///< 共享内存访问器
    
    std::atomic<bool> monitoring_;              ///< 监控状态标志
    std::atomic<bool> initialized_;             ///< 初始化状态标志
    std::thread monitor_thread_;                ///< 监控线程
    
    mutable std::mutex snapshot_mutex_;         ///< 快照数据保护锁
    DDSSystemSnapshot latest_snapshot_;         ///< 最新快照数据
    
    std::function<void(const DDSSystemSnapshot&)> monitor_callback_; ///< 监控数据回调函数
    
    /**
     * @brief 监控线程主循环
     */
    void monitor_loop();
    
    /**
     * @brief 扫描所有Topic信息
     * @param snapshot 输出快照数据
     */
    void scan_topics(DDSSystemSnapshot& snapshot);
    
    /**
     * @brief 检查时间戳是否表示活跃状态
     * @param timestamp 时间戳（纳秒）
     * @param current_time 当前时间（纳秒）
     * @return 活跃返回true，否则返回false
     */
    bool is_active(uint64_t timestamp, uint64_t current_time) const;
    
    /**
     * @brief 获取当前时间戳（纳秒）
     * @return 当前时间戳
     */
    uint64_t get_current_timestamp() const;
    
    /**
     * @brief 计算共享内存使用情况
     * @param snapshot 输出快照数据
     */
    void calculate_memory_usage(DDSSystemSnapshot& snapshot);
};

} // namespace Monitor
} // namespace MB_DDF