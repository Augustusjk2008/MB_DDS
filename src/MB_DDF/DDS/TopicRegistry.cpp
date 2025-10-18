/**
 * @file TopicRegistry.cpp
 * @brief Topic注册表管理类实现
 * @date 2025-08-03
 * @author Jiangkai
 * 
 * 实现Topic的注册、查询和管理功能。
 * 维护Topic元数据信息，支持多进程间的Topic发现。
 */

#include "MB_DDF/DDS/TopicRegistry.h"
#include "MB_DDF/DDS/DDSCore.h"
#include "MB_DDF/DDS/SharedMemory.h"
#include "MB_DDF/Debug/Logger.h"
#include <cstring>

namespace MB_DDF {
namespace DDS {

// 向前声明DDSCore类，用于获取版本号
class DDSCore;

TopicRegistry::TopicRegistry(void* shm_base_addr, size_t shm_size, SharedMemoryManager* shm_manager)
    : shm_base_addr_(shm_base_addr), shm_size_(shm_size), shm_manager_(shm_manager) {
    
    static const uint32_t MAGIC_NUMBER = 0x4C444453; // "LDDS"
    
    // 初始化共享内存中的注册表头部
    header_ = static_cast<TopicRegistryHeader*>(shm_base_addr_);
    
    // 使用信号量确保进程安全的初始化
    sem_t* sem = shm_manager_->get_semaphore();
    if (sem_wait(sem) == -1) {
        LOG_ERROR << "TopicRegistry error waiting for semaphore: " << strerror(errno);
        return; // 构造函数失败
    }
    LOG_DEBUG << "TopicRegistry semaphore acquired";
    
    // 检查魔数，判断是否需要初始化
    if (header_->magic_number != MAGIC_NUMBER) {
        // 第一个进程初始化：将整个共享内存置0
        std::memset(shm_base_addr_, 0, shm_size_);
        
        // 重新设置header指针（因为memset后需要重新初始化）
        header_ = static_cast<TopicRegistryHeader*>(shm_base_addr_);
        
        // 设置魔数标记和版本号
        header_->magic_number = MAGIC_NUMBER;
        header_->version = DDSCore::VERSION;
        header_->next_topic_id.store(1);
        header_->topic_count.store(0);

        LOG_DEBUG << "TopicRegistry initialized with magic number: " << MAGIC_NUMBER;
    } else if (header_->version != DDSCore::VERSION) {
        LOG_ERROR << "DDS version mismatch: expected " << DDSCore::VERSION << ", found " << header_->version;
        return; // 构造函数失败
    }
    
    // 释放信号量
    sem_post(sem);
    
    // 元数据数组位于头部之后
    metadata_array_ = reinterpret_cast<TopicMetadata*>(
        static_cast<char*>(shm_base_addr_) + METADATA_OFFSET);
    
    // 记录初始化日志
    LOG_DEBUG << "TopicRegistry initialized with " << MAX_TOPICS << " slots";
}

TopicMetadata* TopicRegistry::register_topic(const std::string& name, size_t rb_size) {
    // 检查Topic名称是否合法
    if (!is_valid_topic_name(name)) {
        LOG_ERROR << "Invalid topic name format: " << name;
        return nullptr;
    }

    // 使用信号量保护整个操作
    sem_t* sem = shm_manager_->get_semaphore();
    if (sem_wait(sem) == -1) {
        LOG_ERROR << "TopicRegistry error waiting for semaphore: " << strerror(errno);
        return nullptr;
    }
    
    // 首先检查是否已经存在
    TopicMetadata* existing = get_topic_metadata(name);
    if (existing) {
        LOG_WARN << "Topic already registered: " << name;
        sem_post(sem);
        return existing;
    }
    
    // 原子地读取当前计数并检查是否还有空间
    uint32_t current_count = header_->topic_count.load(std::memory_order_acquire);
    if (current_count >= MAX_TOPICS) {
        LOG_ERROR << "Maximum number of topics reached: " << MAX_TOPICS;
        sem_post(sem);
        return nullptr;
    }
    
    // 找到一个空的元数据槽
    TopicMetadata* metadata = nullptr;
    for (size_t i = 0; i < MAX_TOPICS; ++i) {
        if (metadata_array_[i].topic_id == 0) {
            metadata = &metadata_array_[i];
            break;
        }
    }
    
    if (!metadata) {
        LOG_ERROR << "No available metadata slot found";
        sem_post(sem);
        return nullptr;
    }
    
    // 计算对齐后的RingBuffer大小和偏移量，检查溢出
    if (rb_size > SIZE_MAX - ALIGNMENT + 1) {
        LOG_ERROR << "Ring buffer size too large, would cause overflow: " << rb_size;
        sem_post(sem);
        return nullptr;
    }
    size_t aligned_rb_size = ((rb_size + ALIGNMENT - 1) / ALIGNMENT) * ALIGNMENT;
    
    // 计算RingBuffer在共享内存中的偏移量，考虑内存对齐
    size_t rb_offset = DATA_OFFSET;
    for (size_t i = 0; i < current_count; ++i) {
        if (metadata_array_[i].topic_id != 0) {
            size_t existing_size = metadata_array_[i].ring_buffer_size;
            // 检查对齐计算是否会溢出
            if (existing_size > SIZE_MAX - ALIGNMENT + 1) {
                LOG_ERROR << "Existing ring buffer size would cause overflow: " << existing_size;
                sem_post(sem);
                return nullptr;
            }
            size_t existing_aligned_size = ((existing_size + ALIGNMENT - 1) / ALIGNMENT) * ALIGNMENT;
            // 检查偏移量累加是否会溢出
            if (rb_offset > SIZE_MAX - existing_aligned_size) {
                LOG_ERROR << "Ring buffer offset would overflow";
                sem_post(sem);
                return nullptr;
            }
            rb_offset += existing_aligned_size;
        }
    }
    
    // 检查是否超出共享内存边界
    if (rb_offset + aligned_rb_size > shm_size_) {
        LOG_ERROR << "Not enough shared memory for new topic: " << name 
                  << " (required: " << aligned_rb_size << ", available: " << (shm_size_ - rb_offset) << ")";
        sem_post(sem);
        return nullptr;
    }
    
    // 填充元数据
    metadata->topic_id = header_->next_topic_id.fetch_add(1, std::memory_order_acq_rel);
    strncpy(metadata->topic_name, name.c_str(), sizeof(metadata->topic_name) - 1);
    metadata->topic_name[sizeof(metadata->topic_name) - 1] = '\0';
    metadata->ring_buffer_offset = rb_offset;
    metadata->ring_buffer_size = rb_size; // 保存原始大小
    
    // 原子地更新Topic计数
    header_->topic_count.fetch_add(1, std::memory_order_acq_rel);
    
    LOG_INFO << "Registered topic: " << name << " with ID: " << metadata->topic_id 
              << " at offset: " << rb_offset << " (aligned size: " << aligned_rb_size << ")";
    
    sem_post(sem);
    return metadata;
}

TopicMetadata* TopicRegistry::get_topic_metadata(const std::string& name) {
    // 原子地读取topic计数，避免竞态条件
    uint32_t count = header_->topic_count.load(std::memory_order_acquire);
    
    for (size_t i = 0; i < MAX_TOPICS && i < count; ++i) {
        if (metadata_array_[i].topic_id != 0 && 
            strcmp(metadata_array_[i].topic_name, name.c_str()) == 0) {
            return &metadata_array_[i];
        }
    }
    
    LOG_DEBUG << "Topic not found: " << name;
    return nullptr;
}

TopicMetadata* TopicRegistry::get_topic_metadata(uint32_t topic_id) {
    // 原子地读取topic计数，避免竞态条件
    uint32_t count = header_->topic_count.load(std::memory_order_acquire);
    
    for (size_t i = 0; i < MAX_TOPICS && i < count; ++i) {
        if (metadata_array_[i].topic_id == topic_id) {
            return &metadata_array_[i];
        }
    }
    
    LOG_DEBUG << "Topic not found with ID: " << topic_id;
    return nullptr;
}

std::vector<TopicMetadata*> TopicRegistry::get_all_topics() {
    std::vector<TopicMetadata*> topics;
    // 原子地读取topic计数，避免竞态条件
    uint32_t count = header_->topic_count.load(std::memory_order_acquire);
    
    for (size_t i = 0; i < MAX_TOPICS && topics.size() < count; ++i) {
        if (metadata_array_[i].topic_id != 0) {
            topics.push_back(&metadata_array_[i]);
        }
    }
    
    return topics;
}

bool TopicRegistry::is_valid_topic_name(const std::string& name) {
    // 检查名称是否为空
    if (name.empty()) {
        return false;
    }
    
    // 查找 "://" 分隔符
    size_t separator_pos = name.find("://");
    if (separator_pos == std::string::npos) {
        LOG_DEBUG << "Invalid topic name format: " << name << " (missing \"://\")";
        return false;
    }
    
    // 提取域名称（分隔符之前的部分）
    std::string domain = name.substr(0, separator_pos);
    if (domain.empty()) {
        LOG_DEBUG << "Invalid topic name format: " << name << " (empty domain)";
        return false;
    }
    
    // 提取地址名称（分隔符之后的部分）
    std::string address = name.substr(separator_pos + 3);
    if (address.empty()) {
        LOG_DEBUG << "Invalid topic name format: " << name << " (empty address)";
        return false;
    }
    
    return true;
}

} // namespace DDS
} // namespace MB_DDF

