/**
 * @file RingBuffer.cpp
 * @brief 无锁环形缓冲区实现
 * @date 2025-08-03
 * @author Jiangkai
 */

#include "RingBuffer.h"
#include <cstring>
#include <algorithm>
#include <semaphore.h>
#include <sys/time.h>

namespace MB_DDS {
namespace Core {

RingBuffer::RingBuffer(void* buffer, size_t size, sem_t* sem) : sem_(sem) {
    // 计算各部分在共享内存中的布局
    char* base = static_cast<char*>(buffer);
    
    // 头部结构
    header_ = reinterpret_cast<Header*>(base);
    
    // 订阅者注册表
    registry_ = reinterpret_cast<SubscriberRegistry*>(base + sizeof(Header));
    
    // 数据存储区
    size_t metadata_size = sizeof(Header) + sizeof(SubscriberRegistry);
    data_ = base + metadata_size;
    capacity_ = size - metadata_size;
    
    // 初始化头部（仅在首次创建时）
    if (header_->magic_number != Header::MAGIC) {
        new (header_) Header();
        header_->capacity = capacity_;
        header_->data_offset = metadata_size;
        
        // 初始化订阅者注册表
        new (registry_) SubscriberRegistry();
    }
}

bool RingBuffer::publish_message(const Message& message) {
    size_t total_size = calculate_message_total_size(message.header.data_size);
    
    if (!can_write(total_size)) {
        return false;
    }
    
    // 原子性地获取并更新写入位置
    size_t write_pos = header_->write_pos.fetch_add(total_size, std::memory_order_acq_rel);
    size_t actual_pos = write_pos % capacity_;
    
    // 处理环形缓冲区边界情况
    if (actual_pos + total_size > capacity_) {
        // 消息跨越缓冲区边界，需要回绕到开头
        actual_pos = 0;
        header_->write_pos.store(total_size, std::memory_order_release);
    }
    
    // 在缓冲区中构造消息
    Message* buffer_msg = reinterpret_cast<Message*>(data_ + actual_pos);
    
    // 复制消息头部并设置序列号和时间戳
    buffer_msg->header = message.header;
    buffer_msg->header.sequence = header_->current_sequence.fetch_add(1, std::memory_order_acq_rel) + 1;
    buffer_msg->header.set_timestamp();
    
    // 复制数据（如果有）
    if (message.header.data_size > 0 && message.get_data() != nullptr) {
        void* buffer_data = reinterpret_cast<char*>(buffer_msg) + sizeof(Message);
        std::memcpy(buffer_data, message.get_data(), message.header.data_size);
        buffer_msg->set_data(buffer_data);
        
        // 设置校验和
        buffer_msg->header.set_checksum(buffer_data, message.header.data_size);
    } else {
        buffer_msg->set_data(nullptr);
        buffer_msg->header.checksum = 0;
    }
    
    // 内存屏障确保消息完全写入后再通知
    std::atomic_thread_fence(std::memory_order_release);
    
    // 通知订阅者
    notify_subscribers();
    
    return true;
}

bool RingBuffer::read_next_message(uint64_t subscriber_id, Message*& out_message) {
    SubscriberState* state = find_subscriber(subscriber_id);
    if (state == nullptr || !state->active.load(std::memory_order_acquire)) {
        return false;
    }
    
    uint64_t expected_seq = state->next_expected_sequence.load(std::memory_order_acquire);
    uint64_t current_seq = header_->current_sequence.load(std::memory_order_acquire);
    
    // 检查是否有新消息
    if (expected_seq > current_seq) {
        return false;
    }
    
    // 从缓冲区中搜索期望的消息
    size_t write_pos = header_->write_pos.load(std::memory_order_acquire);
    size_t search_start = 0;
    
    // 遍历整个缓冲区寻找匹配序列号的消息
    for (size_t offset = 0; offset < capacity_; offset += ALIGNMENT) {
        size_t pos = (search_start + offset) % capacity_;
        Message* msg = read_message_at(pos);
        
        if (msg != nullptr && validate_message(msg)) {
            // 检查序列号是否匹配期望值
            if (msg->header.sequence == expected_seq) {
                out_message = msg;
                
                // 更新订阅者状态
                state->last_read_sequence.store(msg->header.sequence, std::memory_order_release);
                state->next_expected_sequence.store(msg->header.sequence + 1, std::memory_order_release);
                
                return true;
            }
        }
    }
    
    return false;
}

bool RingBuffer::read_latest_message(uint64_t subscriber_id, Message*& out_message) {
    SubscriberState* state = find_subscriber(subscriber_id);
    if (state == nullptr || !state->active.load(std::memory_order_acquire)) {
        return false;
    }
    
    uint64_t current_seq = header_->current_sequence.load(std::memory_order_acquire);
    if (current_seq == 0) {
        return false;
    }
    
    // 从最新位置向前搜索
    size_t write_pos = header_->write_pos.load(std::memory_order_acquire);
    size_t search_pos = (write_pos > 0) ? write_pos - 1 : capacity_ - 1;
    
    // 向前搜索最新的有效消息
    for (size_t i = 0; i < capacity_; ++i) {
        Message* msg = read_message_at(search_pos);
        if (msg != nullptr && validate_message(msg)) {
            out_message = msg;
            
            // 更新订阅者状态到最新
            state->last_read_sequence.store(msg->header.sequence, std::memory_order_release);
            state->next_expected_sequence.store(msg->header.sequence + 1, std::memory_order_release);
            
            return true;
        }
        
        search_pos = (search_pos > 0) ? search_pos - 1 : capacity_ - 1;
    }
    
    return false;
}

bool RingBuffer::register_subscriber(uint64_t subscriber_id) {
    // 使用信号量保护订阅者注册
    if (sem_wait(sem_) != 0) {
        return false;
    }
    
    bool success = false;
    uint32_t count = registry_->count.load(std::memory_order_acquire);
    
    // 检查是否已存在
    for (uint32_t i = 0; i < count; ++i) {
        if (registry_->subscribers[i].subscriber_id == subscriber_id) {
            registry_->subscribers[i].active.store(true, std::memory_order_release);
            success = true;
            break;
        }
    }
    
    // 添加新订阅者
    if (!success && count < SubscriberRegistry::MAX_SUBSCRIBERS) {
        SubscriberState& new_sub = registry_->subscribers[count];
        new_sub.subscriber_id = subscriber_id;
        new_sub.last_read_sequence.store(0, std::memory_order_release);
        new_sub.next_expected_sequence.store(1, std::memory_order_release);
        new_sub.active.store(true, std::memory_order_release);
        
        registry_->count.store(count + 1, std::memory_order_release);
        success = true;
    }
    
    sem_post(sem_);
    return success;
}

void RingBuffer::unregister_subscriber(uint64_t subscriber_id) {
    // 使用信号量保护订阅者注销
    if (sem_wait(sem_) != 0) {
        return;
    }
    
    uint32_t count = registry_->count.load(std::memory_order_acquire);
    for (uint32_t i = 0; i < count; ++i) {
        if (registry_->subscribers[i].subscriber_id == subscriber_id) {
            registry_->subscribers[i].active.store(false, std::memory_order_release);
            break;
        }
    }
    
    sem_post(sem_);
}

uint32_t RingBuffer::skip_corrupted_messages(uint64_t subscriber_id) {
    SubscriberState* state = find_subscriber(subscriber_id);
    if (state == nullptr || !state->active.load(std::memory_order_acquire)) {
        return 0;
    }
    
    uint32_t skipped = 0;
    size_t search_pos = 0;
    size_t found_pos;
    
    // 从当前位置开始搜索有效消息
    while (find_next_valid_message(search_pos, found_pos)) {
        Message* msg = read_message_at(found_pos);
        if (msg != nullptr && validate_message(msg)) {
            // 找到有效消息，更新订阅者状态
            uint64_t expected_seq = state->next_expected_sequence.load(std::memory_order_acquire);
            if (msg->header.sequence > expected_seq) {
                skipped = msg->header.sequence - expected_seq;
            }
            
            state->next_expected_sequence.store(msg->header.sequence, std::memory_order_release);
            break;
        }
        
        search_pos = found_pos + 1;
        if (search_pos >= capacity_) {
            search_pos = 0;
        }
        ++skipped;
    }
    
    return skipped;
}

bool RingBuffer::wait_for_message(uint64_t subscriber_id, uint32_t timeout_ms) {
    SubscriberState* state = find_subscriber(subscriber_id);
    if (state == nullptr || !state->active.load(std::memory_order_acquire)) {
        return false;
    }
    
    uint32_t current_notification = header_->notification_count.load(std::memory_order_acquire);
    
    // 检查是否已有新消息
    uint64_t expected_seq = state->next_expected_sequence.load(std::memory_order_acquire);
    uint64_t current_seq = header_->current_sequence.load(std::memory_order_acquire);
    
    if (current_seq >= expected_seq) {
        return true;
    }
    
    // 使用futex等待通知
    return futex_wait(&header_->notification_count, current_notification, timeout_ms) == 0;
}

RingBuffer::Statistics RingBuffer::get_statistics() const {
    Statistics stats;
    stats.current_sequence = header_->current_sequence.load(std::memory_order_acquire);
    stats.total_messages = stats.current_sequence;
    stats.corrupted_messages = header_->corrupted_count.load(std::memory_order_acquire);
    
    // 计算可用空间
    size_t write_pos = header_->write_pos.load(std::memory_order_acquire);
    stats.available_space = capacity_ - write_pos;
    
    // 统计活跃订阅者
    uint32_t count = registry_->count.load(std::memory_order_acquire);
    stats.active_subscribers = 0;
    for (uint32_t i = 0; i < count; ++i) {
        if (registry_->subscribers[i].active.load(std::memory_order_acquire)) {
            ++stats.active_subscribers;
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
    
    // 验证魔数
    if (message->header.magic != MessageHeader::MAGIC_NUMBER) {
        return false;
    }
    
    // 验证校验和（使用Message的is_valid方法）
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
    
    return false;
}

void RingBuffer::notify_subscribers() {
    // 增加通知计数并唤醒等待的订阅者
    header_->notification_count.fetch_add(1, std::memory_order_acq_rel);
    futex_wake(&header_->notification_count);
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
    size_t total = sizeof(Message) + data_size;
    // 对齐到ALIGNMENT边界
    return (total + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
}

} // namespace Core
} // namespace MB_DDS