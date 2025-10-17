/**
 * @file Publisher.cpp
 * @brief 发布者类实现
 * @date 2025-08-03
 * @author Jiangkai
 */

#include "MB_DDF/DDS/Publisher.h"
#include <random>

namespace MB_DDF {
namespace DDS {

Publisher::Publisher(TopicMetadata* metadata, RingBuffer* ring_buffer, const std::string& publisher_name)
    : metadata_(metadata), ring_buffer_(ring_buffer), sequence_number_(0), publisher_name_(publisher_name) {
    // 构造函数直接绑定metadata，不需要判断topic是否存在
    // 生成唯一的发布者ID
    std::random_device rd;
    std::mt19937_64 gen(rd());
    publisher_id_ = gen();
    
    // 如果没有提供发布者名称，生成默认名称
    if (publisher_name_.empty()) {
        publisher_name_ = "publisher_" + std::to_string(publisher_id_);
    }
}

Publisher::~Publisher() {
    // 清理资源，当前实现中没有需要特别清理的资源
    // ring_buffer_由外部管理，不需要在这里释放
}

bool Publisher::publish(const void* data, size_t size) {
    if (ring_buffer_ == nullptr) {
        return false;
    }
    
    // 调用RingBuffer的publish_message方法发布消息
    return ring_buffer_->publish_message(data, size);
}

bool Publisher::write(const void* data, size_t size) {
    return publish(data, size);
}

uint32_t Publisher::get_topic_id() const {
    if (metadata_ != nullptr) {
        return metadata_->topic_id;
    }
    return 0; // metadata为空时返回0
}

std::string Publisher::get_topic_name() const {
    if (metadata_ != nullptr) {
        return std::string(metadata_->topic_name);
    }
    return ""; // metadata为空时返回空字符串
}

uint64_t Publisher::get_id() const {
    return publisher_id_;
}

std::string Publisher::get_name() const {
    return publisher_name_;
}

} // namespace DDS
} // namespace MB_DDF