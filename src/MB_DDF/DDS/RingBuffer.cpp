/**
 * @file RingBuffer.cpp
 * @brief 无锁环形缓冲区实现
 * @date 2025-08-03
 * @author Jiangkai
 */

#include "MB_DDF/DDS/RingBuffer.h"
#include "MB_DDF/Debug/Logger.h"
#include <cstring>
#include <semaphore.h>
#include <sys/time.h>

namespace MB_DDF {
namespace DDS {

RingBuffer::RingBuffer(void* buffer, size_t size, sem_t* sem) : sem_(sem) {
    // 计算各部分在共享内存中的布局
    char* base = static_cast<char*>(buffer);
    LOG_DEBUG << "RingBuffer buffer address: " << (void*)base;
    
    // 头部结构
    header_ = reinterpret_cast<RingHeader*>(base);
    
    // 订阅者注册表
    registry_ = reinterpret_cast<SubscriberRegistry*>(base + sizeof(RingHeader));
    
    // 数据存储区
    size_t metadata_size = sizeof(RingHeader) + sizeof(SubscriberRegistry);
    data_ = base + metadata_size;
    capacity_ = size - metadata_size;
    
    // 初始化头部（仅在首次创建时）
    if (header_->magic_number != RingHeader::MAGIC) {
        new (header_) RingHeader();
        header_->capacity = capacity_;
        header_->data_offset = metadata_size;
        
        // 初始化订阅者注册表
        new (registry_) SubscriberRegistry();
    }

    LOG_DEBUG << "RingBuffer created with capacity " << capacity_ << " and data offset " << header_->data_offset;
}

bool RingBuffer::publish_message(const void* data, size_t size) {
    size_t total_size = calculate_message_total_size(size);
    
    if (!can_write(total_size)) {
        LOG_ERROR << "publish_message failed, not enough space";
        return false;
    }
    
    // 当前需要写入的位置 to_write_pos
    size_t to_write_pos = header_->write_pos.load(std::memory_order_acquire) % capacity_;
    
    // 在缓冲区中构造消息
    Message* buffer_msg = reinterpret_cast<Message*>(data_ + to_write_pos);
    
    // 复制消息头部并设置序列号和时间戳
    buffer_msg->header = MessageHeader();
    buffer_msg->header.sequence = header_->current_sequence.fetch_add(1, std::memory_order_acq_rel) + 1;
    
    // 写入数据（如有）
    if (data != nullptr) {
        buffer_msg->header.data_size = size;
        void* buffer_data = buffer_msg->get_data();
        std::memcpy(buffer_data, data, size);
    } else {
        // 设置消息长度为0
        buffer_msg->header.data_size = 0;
    }

    // 更新消息时戳和校验和
    buffer_msg->update();

    // 对齐到 ALIGNMENT
    size_t new_write_pos = (to_write_pos + total_size) % capacity_;
    // 确保新的写入位置对齐到ALIGNMENT边界
    new_write_pos = (new_write_pos + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
    if (new_write_pos >= capacity_) {
        new_write_pos = 0; // 回绕到缓冲区开始
    }
    
    // 更新当前序列号和写入位置
    header_->current_sequence.store(buffer_msg->header.sequence, std::memory_order_release);
    header_->write_pos.store(new_write_pos, std::memory_order_release);
    // 更新最新消息时间戳
    header_->timestamp.store(buffer_msg->header.timestamp, std::memory_order_release);
    
    // 内存屏障确保消息完全写入后再通知
    std::atomic_thread_fence(std::memory_order_release);
    
    // 通知订阅者
    notify_subscribers();
    
    LOG_DEBUG << "publish_message " << buffer_msg->header.sequence << " with size " << size;
    return true;
}

bool RingBuffer::read_expected(SubscriberState* subscriber, Message*& out_message, uint64_t next_expected_sequence) {
    if (subscriber == nullptr) {
        LOG_ERROR << "read_expected failed, subscriber is nullptr";
        return false;
    }
    
    uint64_t buffer_current_seq = header_->current_sequence.load(std::memory_order_acquire);
    
    // 检查是否有新消息
    if (next_expected_sequence > buffer_current_seq) {
        LOG_DEBUG << "read_expected failed, next_expected_sequence " << next_expected_sequence << " > buffer_current_seq " << buffer_current_seq;
        return false;
    }
    
    // 从起始位置开始搜索期望的消息
    size_t search_pos = subscriber->read_pos;
    for (size_t i = 0; i < capacity_; i += ALIGNMENT) {
        Message* msg = read_message_at(search_pos);
        
        if (validate_message(msg)) {
            // 检查序列号是否匹配期望值
            if (msg->header.sequence == next_expected_sequence) {
                out_message = msg;
                subscriber->last_read_sequence = msg->header.sequence;
                subscriber->read_pos = search_pos;
                subscriber->timestamp = msg->header.timestamp;
                return true;
            } else {
                search_pos = (search_pos + msg->msg_size()) % capacity_;
                // 对齐到ALIGNMENT边界
                search_pos = (search_pos + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
            }
        } else {
            search_pos = (search_pos + ALIGNMENT) % capacity_;
        }        
    }
    
    LOG_DEBUG << "read_expected failed, next_expected_sequence " << next_expected_sequence << " not found";
    return false;
}

bool RingBuffer::read_next(SubscriberState* subscriber, Message*& out_message) {
    return read_expected(subscriber, out_message, subscriber->last_read_sequence + 1);
}

uint64_t RingBuffer::get_unread_count(SubscriberState* subscriber) {
    if (subscriber == nullptr) {
        LOG_ERROR << "get_unread_count failed, subscriber is nullptr";
        return 0;
    }

    uint64_t buffer_current_seq = header_->current_sequence.load(std::memory_order_acquire);
    
    // 如果缓冲区当前序号小于等于已读序号，说明没有未读消息
    if (buffer_current_seq <= subscriber->last_read_sequence) {
        LOG_DEBUG << "get_unread_count failed, buffer_current_seq " << buffer_current_seq << " <= last_read_sequence " << subscriber->last_read_sequence;
        return 0;
    }
    
    // 计算未读消息数量
    uint64_t unread_count = buffer_current_seq - subscriber->last_read_sequence;
    LOG_DEBUG << "get_unread_count " << unread_count;
    return unread_count;
}

bool RingBuffer::read_latest(SubscriberState* subscriber, Message*& out_message) {
    return read_expected(subscriber, out_message, header_->current_sequence.load(std::memory_order_acquire));
}

bool RingBuffer::set_publisher(uint64_t publisher_id, const std::string& publisher_name) {
    // 检查是否已存在
    if (header_->publisher_id != 0) {
        LOG_ERROR << "set_publisher failed, publisher already registered";
        // 如果名字相同，允许更新id
        if (std::strcmp(header_->publisher_name, publisher_name.c_str()) == 0) {
            header_->publisher_id = publisher_id;
            LOG_INFO << "set_publisher " << publisher_id << " " << publisher_name << " (name unchanged)";
            return true;
        }
        return false;
    }

    // 添加新发布者
    header_->publisher_id = publisher_id;
    // 复制发布者名称，确保不超过缓冲区大小
    size_t name_len = std::min(publisher_name.length(), sizeof(header_->publisher_name) - 1);
    std::strncpy(header_->publisher_name, publisher_name.c_str(), name_len);
    header_->publisher_name[name_len] = '\0';

    LOG_INFO << "set_publisher " << publisher_id << " " << publisher_name;
    return true;
}

void RingBuffer::remove_publisher() {
    header_->publisher_id = 0;
    header_->publisher_name[0] = '\0';
    LOG_INFO << "remove_publisher";
}

SubscriberState* RingBuffer::register_subscriber(uint64_t subscriber_id, const std::string& subscriber_name) {
    // 使用信号量保护订阅者注册
    if (sem_wait(sem_) != 0) {
        LOG_ERROR << "register_subscriber failed, sem_wait failed";
        return nullptr;
    }
    
    bool success = false;
    uint32_t count = registry_->count.load(std::memory_order_acquire);
    LOG_DEBUG << "register_subscriber count " << count;
    
    // 检查是否已存在
    for (uint32_t i = 0; i < count; ++i) {
        if (registry_->subscribers[i].subscriber_id == subscriber_id) {
            LOG_ERROR << "register_subscriber failed, subscriber_id " << subscriber_id << " already registered";
            return &registry_->subscribers[i];
        }
    }

    // 有可能名字重复，但是id不同
    for (uint32_t i = 0; i < count; ++i) {
        if (std::strcmp(registry_->subscribers[i].subscriber_name, subscriber_name.c_str()) == 0) {
            LOG_ERROR << "register_subscriber failed, subscriber_name " << subscriber_name << " already registered";
            registry_->subscribers[i].subscriber_id = subscriber_id;
            return &registry_->subscribers[i];
        }
    }

    // 查找空闲插槽
    uint32_t free_index = 0;
    for (uint32_t i = 0; i < SubscriberRegistry::MAX_SUBSCRIBERS; ++i) {
        if (registry_->subscribers[i].subscriber_id == 0) {
            free_index = i;
            break;
        }
    }

    // 是否有空闲插槽
    if (free_index == SubscriberRegistry::MAX_SUBSCRIBERS) {
        LOG_ERROR << "register_subscriber failed, no free subscriber slot";
        return nullptr;
    }
    
    // 添加新订阅者
    if (!success && count < SubscriberRegistry::MAX_SUBSCRIBERS) {
        SubscriberState& new_sub = registry_->subscribers[free_index];
        new_sub.subscriber_id = subscriber_id;
        
        // 复制订阅者名称，确保不超过缓冲区大小
        size_t name_len = std::min(subscriber_name.length(), sizeof(new_sub.subscriber_name) - 1);
        std::strncpy(new_sub.subscriber_name, subscriber_name.c_str(), name_len);
        new_sub.subscriber_name[name_len] = '\0';
        
        new_sub.read_pos.store(0, std::memory_order_release);
        new_sub.last_read_sequence.store(0, std::memory_order_release);
        new_sub.timestamp.store(0, std::memory_order_release);
        
        registry_->count.store(count + 1, std::memory_order_release);
        success = true;
        LOG_DEBUG << "register_subscriber " << subscriber_id << " " << subscriber_name;
    }
    
    sem_post(sem_);
    return success ? &registry_->subscribers[free_index] : nullptr;
}

void RingBuffer::unregister_subscriber(SubscriberState* subscriber) {
    // 使用信号量保护订阅者注销
    if (sem_wait(sem_) != 0) {
        LOG_ERROR << "unregister_subscriber failed, sem_wait failed";
        return;
    }
    
    uint32_t count = registry_->count.load(std::memory_order_acquire);
    for (uint32_t i = 0; i < count; ++i) {
        if (registry_->subscribers[i].subscriber_id == subscriber->subscriber_id) {
            registry_->subscribers[i].subscriber_id = 0;
            registry_->subscribers[i].read_pos.store(0, std::memory_order_release);
            registry_->subscribers[i].last_read_sequence.store(0, std::memory_order_release);
            registry_->subscribers[i].timestamp.store(0, std::memory_order_release);
            LOG_INFO << "unregister_subscriber " << subscriber->subscriber_id << " " << registry_->subscribers[i].subscriber_name;
            break;
        }
    }
    
    registry_->count.store(count - 1, std::memory_order_release);
    sem_post(sem_);
}

bool RingBuffer::wait_for_message(SubscriberState* subscriber, uint32_t timeout_ms) {
    if (subscriber == nullptr) {
        LOG_ERROR << "wait_for_message failed, subscriber is nullptr";
        return false;
    }
    
    uint32_t current_notification = header_->notification_count.load(std::memory_order_acquire);
    
    // 检查是否已有新消息
    uint64_t expected_seq = subscriber->last_read_sequence.load(std::memory_order_acquire) + 1;
    uint64_t current_seq = header_->current_sequence.load(std::memory_order_acquire);
    
    if (current_seq >= expected_seq) {
        LOG_INFO << "wait_for_message " << subscriber->subscriber_id << " " << current_seq << " " << expected_seq;
        return true;
    }
    
    // 使用futex等待通知
    LOG_DEBUG << current_seq << " wait_for_message " << expected_seq << " time_out " << timeout_ms;
    return futex_wait(reinterpret_cast<volatile uint32_t*>(&header_->notification_count), current_notification, timeout_ms) == 0;
}

bool RingBuffer::empty() const {
    return header_->current_sequence.load(std::memory_order_acquire) == 0;
}

bool RingBuffer::full() const {
    // 缓冲区可以被覆盖，所以永远不会满
    return false;
}

size_t RingBuffer::available_space() const {
    // 总是有空间可写（通过覆盖实现）
    return capacity_;
}

size_t RingBuffer::available_data() const {
    return header_->current_sequence.load(std::memory_order_acquire);
}

RingBuffer::Statistics RingBuffer::get_statistics() const {
    Statistics stats;
    stats.current_sequence = header_->current_sequence.load(std::memory_order_acquire);
    stats.total_messages = stats.current_sequence;
    
    // 计算可用空间
    size_t write_pos = header_->write_pos.load(std::memory_order_acquire);
    stats.available_space = capacity_ - write_pos;
    
    // 统计活跃订阅者
    uint32_t count = registry_->count.load(std::memory_order_acquire);
    stats.active_subscribers = 0;
    for (uint32_t i = 0; i < count; ++i) {
        if (registry_->subscribers[i].subscriber_id != 0) {
            ++stats.active_subscribers;
        }
    }

    // 收集订阅者名称和ID
    stats.subscribers.clear();
    for (uint32_t i = 0; i < count; ++i) {
        if (registry_->subscribers[i].subscriber_id != 0) {
            stats.subscribers.push_back({
                registry_->subscribers[i].subscriber_id, 
                registry_->subscribers[i].subscriber_name});
        }
    }
    
    return stats;
}

// 私有方法实现
bool RingBuffer::can_write(size_t message_size) const {
    if (message_size > capacity_) {
        return false;
    } else {
        return true;
    }
}

SubscriberState* RingBuffer::find_subscriber(uint64_t subscriber_id) const {
    uint32_t count = registry_->count.load(std::memory_order_acquire);
    for (uint32_t i = 0; i < count; ++i) {
        if (registry_->subscribers[i].subscriber_id == subscriber_id) {
            return &registry_->subscribers[i];
        }
    }
    return nullptr;
}

Message* RingBuffer::read_message_at(size_t pos) const {
    if (pos >= capacity_) {
        return nullptr;
    }
    
    return reinterpret_cast<Message*>(data_ + pos);
}

bool RingBuffer::validate_message(const Message* message) const {
    if (message == nullptr) {
        return false;
    }
    
    // 验证Message
    if (!message->is_valid()) {
        return false;
    }
    
    // 验证数据大小合理性
    if (message->header.data_size > capacity_) {
        return false;
    }
    
    return true;
}

bool RingBuffer::find_next_valid_message(size_t start_pos, size_t& out_pos) const {
    for (size_t i = 0; i < capacity_; i += ALIGNMENT) {
        size_t pos = (start_pos + i) % capacity_;
        Message* msg = read_message_at(pos);
        
        if (msg != nullptr && validate_message(msg)) {
            out_pos = pos;
            return true;
        }
    }
    
    LOG_DEBUG << "find_next_valid_message failed, no valid message found";
    return false;
}

void RingBuffer::notify_subscribers() {
    // 增加通知计数并唤醒等待的订阅者
    header_->notification_count.fetch_add(1, std::memory_order_acq_rel);
    futex_wake(reinterpret_cast<volatile uint32_t*>(&header_->notification_count));
}

int RingBuffer::futex_wait(volatile uint32_t* addr, uint32_t expected, uint32_t timeout_ms) {
    struct timespec timeout;
    struct timespec* timeout_ptr = nullptr;
    
    if (timeout_ms > 0) {
        timeout.tv_sec = timeout_ms / 1000;
        timeout.tv_nsec = (timeout_ms % 1000) * 1000000;
        timeout_ptr = &timeout;
    }
    
    return syscall(SYS_futex, addr, FUTEX_WAIT, expected, timeout_ptr, nullptr, 0);
}

int RingBuffer::futex_wake(volatile uint32_t* addr, uint32_t count) {
    return syscall(SYS_futex, addr, FUTEX_WAKE, count, nullptr, nullptr, 0);
}

size_t RingBuffer::calculate_message_total_size(size_t data_size) {
    size_t total = Message::total_size(data_size);
    // 对齐到ALIGNMENT边界
    return (total + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
}

} // namespace DDS
} // namespace MB_DDF