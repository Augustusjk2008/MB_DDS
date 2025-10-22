/**
 * @file Publisher.h
 * @brief 发布者类定义
 * @date 2025-08-03
 * @author Jiangkai
 * 
 * 提供消息发布功能，支持将数据发布到指定Topic的环形缓冲区中。
 * 每个发布者维护独立的序列号，确保消息的有序性。
 */

#pragma once

#include "MB_DDF/DDS/RingBuffer.h"
#include "MB_DDF/DDS/TopicRegistry.h"
#include <string>
#include <cstdint>

namespace MB_DDF {
namespace DDS {

/**
 * @class Publisher
 * @brief 消息发布者类
 * 
 * 负责将用户数据封装成消息并发布到指定Topic的环形缓冲区中。
 * 自动管理消息序列号和时间戳，确保消息的完整性和有序性。
 */
class Publisher {
public:
    /**
     * @brief 构造函数
     * @param metadata Topic元数据指针
     * @param ring_buffer 关联的环形缓冲区指针
     * @param publisher_name 发布者名称（可选，默认为空）
     */
    Publisher(TopicMetadata* metadata, RingBuffer* ring_buffer, const std::string& publisher_name = "");
    
    /**
     * @brief 析构函数，清理资源
     */
    ~Publisher();

    /**
     * @brief 发布数据到Topic
     * @param data 要发布的数据指针
     * @param size 数据大小（字节）
     * @return 发布成功返回true，失败返回false
     */
    bool publish(const void* data, size_t size);

    /**
     * @brief 写入数据到Topic（别名）
     * @param data 要写入的数据指针
     * @param size 数据大小（字节）
     * @return 写入成功返回true，失败返回false
     */
    bool write(const void* data, size_t size);

    /**
     * @brief 获取Topic ID
     * @return Topic的唯一标识符
     */
    uint32_t get_topic_id() const;

    /**
     * @brief 获取Topic名称
     * @return Topic名称字符串
     */
    std::string get_topic_name() const;

    /**
     * @brief 获取发布者ID
     * @return 发布者的唯一标识符
     */
    uint64_t get_id() const;

    /**
     * @brief 获取发布者名称
     * @return 发布者名称字符串
     */
    std::string get_name() const;

private:
    TopicMetadata* metadata_;    ///< Topic元数据指针
    RingBuffer* ring_buffer_;    ///< 环形缓冲区指针

    // 自身信息
    uint64_t publisher_id_;        ///< 唯一的发布者ID
    std::string publisher_name_;   ///< 发布者名称
};

} // namespace DDS
} // namespace MB_DDF



