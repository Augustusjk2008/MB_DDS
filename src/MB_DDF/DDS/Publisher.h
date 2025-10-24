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
#include <functional>

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
     * @brief 零拷贝写槽句柄（RAII）。在析构未提交时自动取消。
     */
    class WritableMessage {
    public:
        WritableMessage(RingBuffer* rb, TopicMetadata* metadata, const RingBuffer::ReserveToken& token);
        ~WritableMessage();
        void* data();
        size_t capacity() const;
        bool commit(size_t used);
        void cancel();
        bool valid() const;
    private:
        RingBuffer* rb_;
        TopicMetadata* metadata_;
        RingBuffer::ReserveToken token_;
        bool committed_;
    };

    /**
     * @brief 开始一条零拷贝消息，返回可写句柄
     * @param max_size 载荷最大预留大小
     * @return 写槽句柄（invalid时表示预留失败）
     */
    WritableMessage begin_message(size_t max_size);

    /**
     * @brief 使用回调填充并发布零拷贝消息
     * @param max_size 载荷最大预留大小
     * @param fill 用户回调，签名为 size_t(void* buffer, size_t capacity)，返回写入字节数，返回0表示取消
     * @return 发布成功返回true
     */
    bool publish_fill(size_t max_size, const std::function<size_t(void* buffer, size_t capacity)>& fill);

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



