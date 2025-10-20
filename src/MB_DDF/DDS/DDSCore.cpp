/**
 * @file DDSCore.cpp
 * @brief DDSCore主接口类实现
 * @date 2025-08-03
 * @author Jiangkai
 * 
 * 实现DDSCore（数据分发服务）的主要接口，支持发布者-订阅者模式的消息传递。
 * 基于共享内存和无锁环形缓冲区实现高性能的进程间通信。
 */

#include "MB_DDF/DDS/DDSCore.h"
#include "MB_DDF/Debug/Logger.h"

#include <iostream>
#include <fstream>
#include <string>

namespace MB_DDF {
namespace DDS {

// 定义静态成员变量
const uint32_t DDSCore::VERSION;

DDSCore& DDSCore::instance() {
    static DDSCore instance;
    return instance;
}

std::shared_ptr<Publisher> DDSCore::create_publisher(const std::string& topic_name) {     
    // 等待信号量，确保对共享内存的访问是互斥的
    if (sem_wait(shm_manager_->get_semaphore()) != 0) {
        LOG_ERROR << "create_or_get_topic_buffer failed, sem_wait failed";
        return nullptr;
    }

    // 创建或获取环形缓冲区
    RingBuffer* buffer = create_or_get_topic_buffer(topic_name);
    sem_post(shm_manager_->get_semaphore()); // 发布信号量，允许其他进程访问共享内存
    if (buffer == nullptr) {
        LOG_ERROR << "failed to create or get topic buffer, topic name: " << topic_name;
        return nullptr; // 未找到匹配的环形缓冲区
    }

    // 查找TopicMetadata
    TopicMetadata* metadata = find_topic(topic_name);
    if (metadata == nullptr) {
        LOG_ERROR << "failed to find topic metadata, topic name: " << topic_name;
        return nullptr; // 未找到匹配的TopicMetadata
    }
    
    LOG_INFO << "created publisher, topic name: " << topic_name;
    auto publisher = std::make_shared<Publisher>(metadata, buffer, process_name_);
    if (!buffer->set_publisher(publisher->get_id(), publisher->get_name())) {
        LOG_ERROR << "failed to set publisher, publisher id: " << publisher->get_id() << ", name: " << publisher->get_name();
        return nullptr; 
    }
    return publisher;
}

std::shared_ptr<Publisher> DDSCore::create_writer(const std::string& topic_name) {   
    return create_publisher(topic_name);
} 

std::shared_ptr<Subscriber> DDSCore::create_subscriber(const std::string& topic_name, const MessageCallback& callback) {
    // 等待信号量，确保对共享内存的访问是互斥的
    if (sem_wait(shm_manager_->get_semaphore()) != 0) {
        LOG_ERROR << "create_or_get_topic_buffer failed, sem_wait failed";
        return nullptr;
    }
    
    // 创建或获取环形缓冲区
    RingBuffer* buffer = create_or_get_topic_buffer(topic_name);
    sem_post(shm_manager_->get_semaphore()); // 发布信号量，允许其他进程访问共享内存
    if (buffer == nullptr) {
        LOG_ERROR << "failed to create or get topic buffer, topic name: " << topic_name;
        return nullptr; // 未找到匹配的环形缓冲区
    }

    // 查找TopicMetadata
    TopicMetadata* metadata = find_topic(topic_name);
    if (metadata == nullptr) {
        LOG_ERROR << "failed to find topic metadata, topic name: " << topic_name;
        return nullptr; // 未找到匹配的TopicMetadata
    }
    
    LOG_INFO << "created subscriber, topic name: " << topic_name;
    std::shared_ptr<Subscriber> subscriber = std::make_shared<Subscriber>(metadata, buffer, process_name_);
    subscriber->subscribe(callback);
    return subscriber;
}

std::shared_ptr<Subscriber> DDSCore::create_reader(const std::string& topic_name, const MessageCallback& callback) {
    return create_subscriber(topic_name, callback);
}

size_t DDSCore::data_write(std::shared_ptr<Publisher> publisher, const void* data, size_t size) {
    return publisher->write(data, size);
}

size_t DDSCore::data_read(std::shared_ptr<Subscriber> subscriber, void* data, size_t size) {
    return subscriber->read(data, size);
}

bool DDSCore::initialize(size_t shared_memory_size) {
    // 检查是否已经初始化
    if (initialized_) {
        LOG_WARN << "already initialized, shared memory size: " << shm_manager_->get_size();
        return true; // 已经初始化，直接返回成功
    }
    
    // 参数验证
    if (shared_memory_size < 1024 * 1024) { // 最小1MB
        LOG_ERROR << "Shared_memory_size too small (minimum 1MB), size: " << shared_memory_size;
        return false;
    }
    
    try {
        // 1. 创建共享内存管理器
        shm_manager_ = std::make_unique<SharedMemoryManager>("/MB_DDF_SHM", shared_memory_size);
        
        // 检查共享内存是否创建成功
        if (!shm_manager_ || !shm_manager_->get_address()) {
            LOG_ERROR << "Failed to create shared memory manager";
            shm_manager_.reset();
            return false;
        }
        LOG_DEBUG << "Shared memory created, size: " << shm_manager_->get_size();
        
        // 2. 创建Topic注册表
        topic_registry_ = std::make_unique<TopicRegistry>(
            shm_manager_->get_address(), 
            shm_manager_->get_size(), 
            shm_manager_.get()
        );
        LOG_DEBUG << "topic registry created";
        
        // 检查Topic注册表是否创建成功
        if (!topic_registry_) {
            LOG_ERROR << "Failed to create topic registry";
            shm_manager_.reset();
            return false;
        }
        LOG_DEBUG << "topic registry initialized";
        
        // 3. 初始化其他成员变量
        topic_buffers_.clear();
        process_name_ = get_process_name();
        initialized_ = true;

        LOG_INFO << "DDSCore initialized successfully with " << shared_memory_size << " bytes shared memory";
        return true;
        
    } catch (const std::exception& e) {
        // 清理已分配的资源
        LOG_ERROR << "Exception occurred: " << e.what();
        topic_registry_.reset();
        shm_manager_.reset();
        initialized_ = false;
        return false;
    } catch (...) {
        // 捕获所有其他异常
        LOG_ERROR << "Unknown exception occurred";
        topic_registry_.reset();
        shm_manager_.reset();
        initialized_ = false;
        return false;
    }
}

RingBuffer* DDSCore::create_or_get_topic_buffer(const std::string& topic_name) {
    // 检查系统是否已初始化
    if (!initialized_) {
        initialize();
    }
    
    // 验证topic名称
    if (!topic_registry_->is_valid_topic_name(topic_name)) {
        LOG_ERROR << "Invalid topic name: " << topic_name;
        return nullptr;
    }
    
    // 首先尝试从TopicRegistry获取已存在的Topic元数据
    TopicMetadata* metadata = topic_registry_->get_topic_metadata(topic_name);
    
    if (metadata != nullptr) {
        // Topic已存在，检查是否已经在topic_buffers_中
        auto it = topic_buffers_.find(metadata);
        if (it != topic_buffers_.end()) {
            // RingBuffer已存在，直接返回
            LOG_DEBUG << "Retrieved existing RingBuffer for topic: " << topic_name;
            return it->second.get();
        } else {
            // Topic存在但RingBuffer未创建，需要创建RingBuffer
            try {
                // 计算环形缓冲区在共享内存中的地址
                void* buffer_addr = static_cast<char*>(shm_manager_->get_address()) + metadata->ring_buffer_offset;
                
                // 创建RingBuffer实例
                auto ring_buffer = std::make_unique<RingBuffer>(
                    buffer_addr, 
                    metadata->ring_buffer_size, 
                    shm_manager_->get_semaphore()
                );
                
                // 将RingBuffer添加到映射中
                RingBuffer* buffer_ptr = ring_buffer.get();
                topic_buffers_[metadata] = std::move(ring_buffer);
                LOG_DEBUG << "Created RingBuffer for existing topic: " << topic_name;
                return buffer_ptr;
                                
            } catch (const std::exception& e) {
                LOG_ERROR << "Failed to create RingBuffer for existing topic: " 
                         << e.what();
                return nullptr;
            }
        }
    } else {
        // Topic不存在，需要创建新的Topic
        try {
            // 默认环形缓冲区大小为1MB
            const size_t ring_buffer_size = 1024 * 1024;
            
            // 在TopicRegistry中注册新的Topic
            metadata = topic_registry_->register_topic(topic_name, ring_buffer_size);
            if (!metadata) {
                LOG_ERROR << "Failed to register new topic: " << topic_name;
                return nullptr;
            }
            
            // 计算环形缓冲区在共享内存中的地址
            void* buffer_addr = static_cast<char*>(shm_manager_->get_address()) + metadata->ring_buffer_offset;
            
            // 创建RingBuffer实例
            auto ring_buffer = std::make_unique<RingBuffer>(
                buffer_addr, 
                metadata->ring_buffer_size, 
                shm_manager_->get_semaphore()
            );
            
            // 将RingBuffer添加到映射中
            RingBuffer* buffer_ptr = ring_buffer.get();
            topic_buffers_[metadata] = std::move(ring_buffer);
            LOG_DEBUG << "Created new topic '" << topic_name 
                     << "' with " << ring_buffer_size << " bytes ring buffer";
            
            return buffer_ptr;
            
        } catch (const std::exception& e) {
            LOG_ERROR << "Failed to create new topic: " 
                     << e.what() << std::endl;
            return nullptr;
        }
    }
}

TopicMetadata* DDSCore::find_topic(const std::string& topic_name) {
    // 检查系统是否已初始化
    if (!initialized_) {
        return nullptr;
    }
    
    // 遍历topic_buffers_映射，查找匹配的TopicMetadata
    for (const auto& pair : topic_buffers_) {
        TopicMetadata* metadata = pair.first;
        if (metadata != nullptr && 
            std::string(metadata->topic_name) == topic_name) {
            return metadata;
        }
    }
    
    return nullptr; // 未找到匹配的TopicMetadata
}

std::string DDSCore::get_process_name() {
    std::ifstream comm("/proc/self/comm");
    if (!comm.is_open()) {
        return "unknown"; // 打开失败（如无proc文件系统）
    }
    std::string name;
    std::getline(comm, name);
    return name;
}

} // namespace DDS
} // namespace MB_DDF