/**
 * @file Subscriber.cpp
 * @brief 订阅者类实现
 * @date 2025-08-03
 * @author Jiangkai
 * 
 * 实现消息订阅功能，包括异步消息接收、回调处理和线程管理。
 */
#include "MB_DDF/DDS/Subscriber.h"
#include "MB_DDF/DDS/RingBuffer.h"
#include "MB_DDF/DDS/Message.h"
#include "MB_DDF/Debug/Logger.h"
#include <random>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>

namespace MB_DDF {
namespace DDS {

Subscriber::Subscriber(TopicMetadata* metadata, RingBuffer* ring_buffer, const std::string& subscriber_name, std::shared_ptr<Handle> handle)
    : metadata_(metadata), ring_buffer_(ring_buffer), callback_(nullptr),
      subscribed_(false), running_(false), worker_thread_(), handle_(std::move(handle)),
      subscriber_name_(subscriber_name) {
    // 生成唯一的订阅者ID
    std::random_device rd;
    std::mt19937_64 gen(rd());
    subscriber_id_ = gen();
    
    // 如果没有提供订阅者名称，生成默认名称
    if (subscriber_name_.empty()) {
        subscriber_name_ = "subscriber_" + std::to_string(subscriber_id_);
    }

    // 如果提供了句柄，初始化接收缓存
    if (handle_) {
        receive_buffer_.resize(handle_->getMTU());
    }
}

Subscriber::~Subscriber() {
    unsubscribe();
}

bool Subscriber::subscribe(MessageCallback callback) {
    if (subscribed_.load()) {
        LOG_DEBUG << "Subscriber " << subscriber_id_ << " " << subscriber_name_ << " already subscribed";
        return false; // 已经订阅
    }
    
    if (handle_ == nullptr) {
        // 在RingBuffer中注册订阅者
        subscriber_state_ = ring_buffer_->register_subscriber(subscriber_id_, subscriber_name_);
        if (!subscriber_state_) {
            LOG_DEBUG << "Failed to register subscriber " << subscriber_id_ << " " << subscriber_name_;
            return false;
        }
    }
    
    callback_ = callback;
    subscribed_.store(true);
    running_.store(true);
    
    // 如需实时检测，则设置回调函数，启动工作线程
    if (callback_) {
        LOG_DEBUG << "Subscriber " << subscriber_id_ << " " << subscriber_name_ << " subscribed with callback";
        worker_thread_ = std::thread(&Subscriber::worker_loop, this);
    }
    
    LOG_DEBUG << "Subscriber " << subscriber_id_ << " " << subscriber_name_ << " subscribed successfully";
    return true;
}

void Subscriber::unsubscribe() {
    if (!subscribed_.load()) {
        LOG_DEBUG << "Subscriber " << subscriber_id_ << " " << subscriber_name_ << " not subscribed";
        return;
    }

    // 停止工作线程
    running_.store(false);
    
    // 先唤醒可能在 Futex 中阻塞的线程，防止 join 僵死
    // 此处会引起惊群效益，性能不佳，目前为折衷设计，后续可以改进为使用通知直接唤醒线程
    if (callback_) {
        LOG_DEBUG << "Subscriber " << subscriber_id_ << " " << subscriber_name_ << " notifies subscribers";
        ring_buffer_->notify_subscribers();
    }

    // 标记为未订阅
    subscribed_.store(false);
    
    if (worker_thread_.joinable()) {
        worker_thread_.join();
        LOG_DEBUG << "Subscriber " << subscriber_id_ << " " << subscriber_name_ << " worker thread joined";
    }
    
    // 从RingBuffer中注销订阅者
    ring_buffer_->unregister_subscriber(subscriber_state_);
    LOG_DEBUG << "Subscriber " << subscriber_id_ << " " << subscriber_name_ << " unregistered from ring buffer";
}

void Subscriber::worker_loop() { 
    size_t received_size = 0;   
    while (running_.load()) {
        received_size = 0;
        if (handle_ != nullptr) {
            received_size = handle_->receive(receive_buffer_.data(), receive_buffer_.size(), 100000); 
            // 调用回调函数
            if (callback_ && received_size > 0) { 
                callback_(receive_buffer_.data(), received_size, 0);
            }
            continue;
        }

        if (ring_buffer_->get_unread_count(subscriber_state_) > 0) {
            // 从环形缓冲区读取下一条消息
            Message* msg = nullptr;
            if (ring_buffer_->read_latest(subscriber_state_, msg)) {
                received_size = msg->msg_size();
                LOG_DEBUG << "Subscriber " << subscriber_name_ << " received latest message of total size: " << received_size;
            }
            
            // 解析消息
            if (received_size >= sizeof(MessageHeader)) {   
                // 验证消息头是否有效
                if (msg->is_valid(ring_buffer_->is_checksum_enabled())) {    
                    // 调用回调函数
                    if (callback_) { 
                        LOG_DEBUG << "msg->get_data(): " << msg->get_data(); 
                        LOG_DEBUG << "msg->msg_data_size(): " << msg->msg_data_size(); 
                        LOG_DEBUG << "msg->header.timestamp: " << msg->header.timestamp; 
                        callback_(msg->get_data(), msg->msg_data_size(), msg->header.timestamp);
                    }
                } else {                
                    LOG_ERROR << "Invalid message received on topic: " << metadata_->topic_name;
                }
            } else {
                LOG_ERROR << "Invalid message size received on topic: " << metadata_->topic_name;
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
        LOG_ERROR << "Invalid CPU ID: " << cpu_id << ", available CPUs: 0-" << (num_cpus - 1);
        return false;
    }

    // 检查工作线程是否已启动
    if (!worker_thread_.joinable()) {
        LOG_ERROR << "Worker thread is not running, cannot bind to CPU";
        return false;
    }

    // 设置CPU亲和性
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_id, &cpuset);

    pthread_t thread_handle = worker_thread_.native_handle();
    int result = pthread_setaffinity_np(thread_handle, sizeof(cpu_set_t), &cpuset);
    
    if (result != 0) {
        LOG_ERROR << "Failed to bind subscriber worker thread to CPU " << cpu_id << ": " << strerror(result);
        return false;
    }

    LOG_DEBUG << "Subscriber worker thread bound to CPU " << cpu_id;
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

size_t Subscriber::read(void* data, size_t size, bool latest) {
    // 绑定了回调函数时不允许自行读取
    if (callback_) {
        return 0; 
    }
    // 绑定句柄时直接读取硬件
    if (handle_ != nullptr) {
        return handle_->receive((uint8_t*)data, size);
    }
    if (latest) {
        return read_latest(data, size);
    } else {
        return read_next(data, size);
    }
}

} // namespace DDS
} // namespace MB_DDF
