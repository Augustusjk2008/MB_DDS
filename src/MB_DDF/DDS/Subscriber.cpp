/**
 * @file Subscriber.cpp
 * @brief 订阅者类实现
 * @date 2025-08-03
 * @author Jiangkai
 * 
 * 实现消息订阅功能，包括异步消息接收、回调处理和线程管理。
 */
#include "Subscriber.h"
#include "RingBuffer.h"
#include "Message.h"
#include <iostream>
#include <random>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>

namespace MB_DDF {
namespace DDS {

Subscriber::Subscriber(TopicMetadata* metadata, RingBuffer* ring_buffer, const std::string& subscriber_name)
    : metadata_(metadata), ring_buffer_(ring_buffer), 
      subscribed_(false), running_(false), subscriber_name_(subscriber_name) {
    // 生成唯一的订阅者ID
    std::random_device rd;
    std::mt19937_64 gen(rd());
    subscriber_id_ = gen();
    
    // 如果没有提供订阅者名称，生成默认名称
    if (subscriber_name_.empty()) {
        subscriber_name_ = "subscriber_" + std::to_string(subscriber_id_);
    }
}

Subscriber::~Subscriber() {
    unsubscribe();
}

bool Subscriber::subscribe(MessageCallback callback) {
    if (subscribed_.load()) {
        return false; // 已经订阅
    }
    
    // 在RingBuffer中注册订阅者
    subscriber_state_ = ring_buffer_->register_subscriber(subscriber_id_, subscriber_name_);
    if (!subscriber_state_) {
        return false;
    }
    
    callback_ = callback;
    subscribed_.store(true);
    running_.store(true);
    
    // 如需实时检测，则设置回调函数，启动工作线程
    if (callback_) {
        worker_thread_ = std::thread(&Subscriber::worker_loop, this);
    }
    
    return true;
}

void Subscriber::unsubscribe() {
    if (!subscribed_.load()) {
        return;
    }
    
    // 先唤醒可能在 Futex 中阻塞的线程，防止 join 僵死
    // 此处会引起惊群效益，性能不佳，目前为折衷设计，后续可以改进为使用通知直接唤醒线程
    if (callback_) {
        ring_buffer_->notify_subscribers();
    }

    running_.store(false);
    subscribed_.store(false);
    
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
    
    // 从RingBuffer中注销订阅者
    ring_buffer_->unregister_subscriber(subscriber_state_);
}

void Subscriber::worker_loop() {
    const size_t max_message_size = 64 * 1024; // 64KB最大消息大小
    std::vector<char> buffer(max_message_size);
    
    while (running_.load()) {
        size_t received_size = 0;
        
        if (ring_buffer_->get_unread_count(subscriber_state_) > 0) {
            // 从环形缓冲区读取下一条消息
            Message* msg = nullptr;
            if (ring_buffer_->read_next(subscriber_state_, msg)) {
                received_size = msg->msg_size();
            }
            
            // 解析消息
            if (received_size >= sizeof(MessageHeader)) {                
                // 验证消息头是否有效
                if (msg->is_valid()) {                    
                    // 调用回调函数
                    if (callback_) {
                        callback_(msg->get_data(), msg->msg_data_size(), msg->header.timestamp);
                    }
                } else {
                    std::cerr << "Invalid message received on topic: " << metadata_->topic_name << std::endl;
                }
            }
        } else {
            // 等待通知以避免忙等待
            ring_buffer_->wait_for_message(subscriber_state_);
        }
    }
}

bool Subscriber::bind_to_cpu(int cpu_id) {
    // 获取系统CPU核心数
    int num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
    if (cpu_id < 0 || cpu_id >= num_cpus) {
        std::cerr << "Invalid CPU ID: " << cpu_id << ", available CPUs: 0-" << (num_cpus - 1) << std::endl;
        return false;
    }

    // 检查工作线程是否已启动
    if (!worker_thread_.joinable()) {
        std::cerr << "Worker thread is not running, cannot bind to CPU" << std::endl;
        return false;
    }

    // 设置CPU亲和性
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_id, &cpuset);

    pthread_t thread_handle = worker_thread_.native_handle();
    int result = pthread_setaffinity_np(thread_handle, sizeof(cpu_set_t), &cpuset);
    
    if (result != 0) {
        std::cerr << "Failed to bind subscriber worker thread to CPU " << cpu_id << ": " << strerror(result) << std::endl;
        return false;
    }

    std::cout << "Subscriber worker thread bound to CPU " << cpu_id << std::endl;
    return true;
}

size_t Subscriber::read_next(void* data, size_t size) {
    if (!subscribed_.load()) {
        return 0; // 未订阅
    }
    
    // 读消息
    Message* msg = nullptr;
    if (ring_buffer_->read_next(subscriber_state_, msg)) {
        // 比较数据大小
        if (msg->msg_data_size() < size) {
            size = msg->msg_data_size();
        }
        
        // 复制数据到用户缓冲区
        memcpy(data, msg->get_data(), size);
        return size;
    }

    return 0; // 无消息
}

size_t Subscriber::read_latest(void* data, size_t size) {
    if (!subscribed_.load()) {
        return 0; // 未订阅
    }
    
    // 读最新消息
    Message* msg = nullptr;
    if (ring_buffer_->read_latest(subscriber_state_, msg)) {
        // 比较数据大小
        if (msg->msg_data_size() < size) {
            size = msg->msg_data_size();
        }
        
        // 复制数据到用户缓冲区
        memcpy(data, msg->get_data(), size);
        return size;
    }

    return 0; // 无消息
}

size_t Subscriber::read(void* data, size_t size) {
    return read_latest(data, size);
}

} // namespace DDS
} // namespace MB_DDF

