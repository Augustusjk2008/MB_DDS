/**
 * @file Publisher.cpp
 * @brief 发布者类实现
 * @date 2025-08-03
 * @author Jiangkai
 */

#include "MB_DDF/DDS/Publisher.h"
#include "MB_DDF/Debug/Logger.h"
#include <random>

namespace MB_DDF {
namespace DDS {

Publisher::Publisher(TopicMetadata* metadata, RingBuffer* ring_buffer, const std::string& publisher_name)
    : metadata_(metadata), ring_buffer_(ring_buffer), publisher_name_(publisher_name) {
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

// WritableMessage 实现
Publisher::WritableMessage::WritableMessage(RingBuffer* rb, TopicMetadata* metadata, const RingBuffer::ReserveToken& token)
    : rb_(rb), metadata_(metadata), token_(token), committed_(false) {}

Publisher::WritableMessage::~WritableMessage() {
    if (!committed_ && token_.valid && rb_) {
        rb_->abort(token_);
    }
}

void* Publisher::WritableMessage::data() {
    return token_.valid && token_.msg ? token_.msg->get_data() : nullptr;
}

size_t Publisher::WritableMessage::capacity() const {
    return token_.capacity;
}

bool Publisher::WritableMessage::commit(size_t used) {
    if (!token_.valid || committed_ || rb_ == nullptr || metadata_ == nullptr) {
        return false;
    }
    bool ok = rb_->commit(token_, used, metadata_->topic_id);
    committed_ = ok;
    return ok;
}

void Publisher::WritableMessage::cancel() {
    if (token_.valid && rb_ && !committed_) {
        rb_->abort(token_);
        committed_ = false;
    }
}

bool Publisher::WritableMessage::valid() const {
    return token_.valid && token_.msg != nullptr;
}

Publisher::WritableMessage Publisher::begin_message(size_t max_size) {
    if (ring_buffer_ == nullptr) {
        return WritableMessage(nullptr, nullptr, RingBuffer::ReserveToken());
    }
    auto token = ring_buffer_->reserve(max_size);
    return WritableMessage(ring_buffer_, metadata_, token);
}

bool Publisher::publish_fill(size_t max_size, const std::function<size_t(void* buffer, size_t capacity)>& fill) {
    if (ring_buffer_ == nullptr || metadata_ == nullptr || !fill) {
        LOG_ERROR << "Publisher " << publisher_name_ << " publish_fill invalid parameters";
        return false;
    }
    auto token = ring_buffer_->reserve(max_size);
    if (!token.valid || token.msg == nullptr) {
        LOG_ERROR << "Publisher " << publisher_name_ << " publish_fill reserve token invalid";
        return false;
    }
    void* buf = token.msg->get_data();
    size_t cap = token.capacity;
    size_t written = 0;
    try {
        written = fill(buf, cap);
    } catch (...) {
        LOG_ERROR << "Publisher " << publisher_name_ << " publish_fill exception";
        ring_buffer_->abort(token);
        return false;
    }
    if (written == 0 || written > cap) {
        ring_buffer_->abort(token);
        LOG_ERROR << "Publisher " << publisher_name_ << " publish_fill invalid written size: " << written;
        return false;
    }
    return ring_buffer_->commit(token, written, metadata_->topic_id);
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