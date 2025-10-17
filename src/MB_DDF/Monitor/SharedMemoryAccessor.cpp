/**
 * @file SharedMemoryAccessor.cpp
 * @brief 共享内存直接访问器实现
 * @date 2025-10-16
 * @author Jiangkai
 */

#include "MB_DDF/Monitor/SharedMemoryAccessor.h"
#include "MB_DDF/Debug/Logger.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <algorithm>

namespace MB_DDF {
namespace Monitor {

SharedMemoryAccessor::SharedMemoryAccessor(const std::string& shm_name)
    : shm_name_(shm_name), shm_fd_(-1), shm_addr_(nullptr), 
      shm_size_(0), connected_(false) {
}

SharedMemoryAccessor::~SharedMemoryAccessor() {
    disconnect();
}

bool SharedMemoryAccessor::connect() {
    if (connected_) {
        return true;
    }
    
    if (!open_shared_memory()) {
        LOG_DEBUG << "Failed to open shared memory: " << shm_name_;
        return false;
    }
    
    if (!map_shared_memory()) {
        LOG_DEBUG << "Failed to map shared memory: " << shm_name_;
        close(shm_fd_);
        shm_fd_ = -1;
        return false;
    }
    
    if (!validate_magic_numbers()) {
        LOG_DEBUG << "Invalid magic numbers in shared memory";
        disconnect();
        return false;
    }
    
    if (!parse_memory_layout()) {
        LOG_DEBUG << "Failed to parse memory layout";
        disconnect();
        return false;
    }
    
    connected_ = true;
    LOG_DEBUG << "Successfully connected to shared memory: " << shm_name_;
    return true;
}

void SharedMemoryAccessor::disconnect() {
    if (!connected_) {
        return;
    }
    
    if (shm_addr_ != nullptr && shm_addr_ != MAP_FAILED) {
        munmap(shm_addr_, shm_size_);
        shm_addr_ = nullptr;
    }
    
    if (shm_fd_ != -1) {
        close(shm_fd_);
        shm_fd_ = -1;
    }
    
    shm_size_ = 0;
    connected_ = false;
    
    // 清理布局信息
    memory_layout_ = SharedMemoryLayout();
    
    LOG_DEBUG << "Disconnected from shared memory: " << shm_name_;
}

bool SharedMemoryAccessor::parse_memory_layout() {
    if (!shm_addr_ || shm_size_ == 0) {
        return false;
    }
    
    memory_layout_.base_address = shm_addr_;
    memory_layout_.total_size = shm_size_;
    
    // 解析Topic注册表头部
    memory_layout_.registry_header = static_cast<DDS::TopicRegistryHeader*>(shm_addr_);
    
    // 计算Topic元数据数组位置
    size_t metadata_offset = sizeof(DDS::TopicRegistryHeader);
    memory_layout_.topics_array = reinterpret_cast<DDS::TopicMetadata*>(
        static_cast<char*>(shm_addr_) + metadata_offset);
    
    // 计算数据区偏移
    memory_layout_.data_area_offset = metadata_offset + 
        128 * sizeof(DDS::TopicMetadata); // MAX_TOPICS = 128
    
    return true;
}

std::vector<DDS::TopicMetadata*> SharedMemoryAccessor::get_all_topics() {
    std::vector<DDS::TopicMetadata*> topics;
    
    if (!connected_ || !memory_layout_.registry_header || !memory_layout_.topics_array) {
        return topics;
    }
    
    uint32_t topic_count = memory_layout_.registry_header->topic_count.load();
    
    // 遍历所有Topic元数据
    for (uint32_t i = 0; i < std::min(topic_count, 128u); ++i) {
        DDS::TopicMetadata* topic = &memory_layout_.topics_array[i];
        
        // 检查Topic是否有效（topic_id != 0 且 topic_name不为空）
        if (topic->topic_id != 0 && topic->topic_name[0] != '\0') {
            topics.push_back(topic);
        }
    }
    
    return topics;
}

RingBufferLayout SharedMemoryAccessor::get_ring_buffer_layout(DDS::TopicMetadata* topic_metadata) {
    RingBufferLayout layout;
    
    if (!connected_ || !topic_metadata || topic_metadata->ring_buffer_offset == 0) {
        return layout;
    }
    
    // 计算环形缓冲区地址
    layout.buffer_base = calculate_ring_buffer_address(topic_metadata->ring_buffer_offset);
    layout.buffer_size = topic_metadata->ring_buffer_size;
    
    if (!layout.buffer_base) {
        return layout;
    }
    
    // 解析环形缓冲区结构
    char* buffer_ptr = static_cast<char*>(layout.buffer_base);
    
    // RingHeader在缓冲区开始位置
    layout.header = reinterpret_cast<DDS::RingHeader*>(buffer_ptr);
    
    // 验证魔数
    if (layout.header->magic_number != DDS::RingHeader::MAGIC) {
        LOG_DEBUG << "Invalid ring buffer magic number";
        return RingBufferLayout(); // 返回空布局
    }
    
    // SubscriberRegistry紧跟在RingHeader之后
    size_t registry_offset = sizeof(DDS::RingHeader);
    // 对齐到64字节边界
    registry_offset = (registry_offset + 63) & ~63;
    layout.subscriber_registry = buffer_ptr + registry_offset;
    
    // 数据区在SubscriberRegistry之后
    size_t subscriber_registry_size = sizeof(std::atomic<uint32_t>) + 
        64 * sizeof(DDS::SubscriberState); // MAX_SUBSCRIBERS = 64
    size_t data_offset = registry_offset + subscriber_registry_size;
    // 对齐到8字节边界
    data_offset = (data_offset + 7) & ~7;
    
    layout.data_area = buffer_ptr + data_offset;
    layout.data_capacity = layout.buffer_size - data_offset;
    
    return layout;
}

SharedMemoryAccessor::PublisherData SharedMemoryAccessor::get_publisher_data(const RingBufferLayout& ring_layout) {
    PublisherData pub_data;
    
    if (!ring_layout.header) {
        LOG_ERROR << "Invalid ring buffer layout: header is null";
        return pub_data;
    }
    
    pub_data.publisher_id = ring_layout.header->publisher_id;
    pub_data.publisher_name = safe_read_string(ring_layout.header->publisher_name, 64);
    pub_data.current_sequence = ring_layout.header->current_sequence.load();
    pub_data.is_valid = (pub_data.publisher_id != 0);

    return pub_data;
}

std::vector<SharedMemoryAccessor::SubscriberData> SharedMemoryAccessor::get_subscribers_data(const RingBufferLayout& ring_layout) {
    std::vector<SubscriberData> subscribers;
    
    if (!ring_layout.subscriber_registry) {
        return subscribers;
    }
    
    // 直接访问订阅者注册表结构
    char* registry_ptr = static_cast<char*>(ring_layout.subscriber_registry);
    
    // 读取订阅者数量（第一个字段是atomic<uint32_t> count）
    std::atomic<uint32_t>* count_ptr = reinterpret_cast<std::atomic<uint32_t>*>(registry_ptr);
    uint32_t subscriber_count = count_ptr->load();
    
    // 订阅者状态数组紧跟在count之后
    size_t states_offset = sizeof(std::atomic<uint32_t>);
    // 对齐到64字节边界
    states_offset = (states_offset + 63) & ~63;
    DDS::SubscriberState* states_array = reinterpret_cast<DDS::SubscriberState*>(
        registry_ptr + states_offset);
    
    // 遍历所有订阅者状态
    for (uint32_t i = 0; i < std::min(subscriber_count, 64u); ++i) {
        DDS::SubscriberState* state = &states_array[i];
        
        if (state->subscriber_id != 0) {
            SubscriberData sub_data;
            sub_data.subscriber_id = state->subscriber_id;
            sub_data.subscriber_name = safe_read_string(state->subscriber_name, 64);
            sub_data.read_pos = state->read_pos.load();
            sub_data.last_read_sequence = state->last_read_sequence.load();
            sub_data.last_active_timestamp = state->timestamp.load();
            sub_data.is_valid = true;
            
            subscribers.push_back(sub_data);
        }
    }
    
    return subscribers;
}

SharedMemoryAccessor::RingBufferStats SharedMemoryAccessor::get_ring_buffer_stats(const RingBufferLayout& ring_layout) {
    RingBufferStats stats;
    
    if (!ring_layout.header) {
        return stats;
    }
    
    stats.total_messages = ring_layout.header->current_sequence.load();
    
    // 计算可用空间和已用空间
    uint64_t write_pos = ring_layout.header->write_pos.load();
    stats.used_space = write_pos % ring_layout.data_capacity;
    stats.available_space = ring_layout.data_capacity - stats.used_space;
    
    // 计算活跃订阅者数量
    auto subscribers = get_subscribers_data(ring_layout);
    stats.active_subscribers = static_cast<uint32_t>(subscribers.size());
    
    return stats;
}

bool SharedMemoryAccessor::validate_memory_integrity() {
    if (!connected_ || !memory_layout_.registry_header) {
        return false;
    }
    
    // 验证Topic注册表头部魔数
    if (memory_layout_.registry_header->magic_number != 0x44445352) { // "DDSR"
        LOG_DEBUG << "Invalid topic registry magic number";
        return false;
    }
    
    // 验证所有有效Topic的环形缓冲区
    auto topics = get_all_topics();
    for (auto* topic : topics) {
        RingBufferLayout ring_layout = get_ring_buffer_layout(topic);
        if (ring_layout.header && 
            ring_layout.header->magic_number != DDS::RingHeader::MAGIC) {
            LOG_DEBUG << "Invalid ring buffer magic number for topic: " << topic->topic_name;
            return false;
        }
    }
    
    return true;
}

SharedMemoryAccessor::MemoryUsageStats SharedMemoryAccessor::get_memory_usage_stats() {
    MemoryUsageStats stats;
    
    if (!connected_) {
        return stats;
    }
    
    stats.total_size = memory_layout_.total_size;
    stats.registry_size = sizeof(DDS::TopicRegistryHeader);
    stats.topics_metadata_size = 128 * sizeof(DDS::TopicMetadata);
    
    // 计算所有环形缓冲区的大小
    auto topics = get_all_topics();
    for (auto* topic : topics) {
        stats.ring_buffers_size += topic->ring_buffer_size;
    }
    
    stats.free_space = stats.total_size - stats.registry_size - 
                      stats.topics_metadata_size - stats.ring_buffers_size;
    
    return stats;
}

bool SharedMemoryAccessor::open_shared_memory() {
    shm_fd_ = shm_open(shm_name_.c_str(), O_RDONLY, 0);
    if (shm_fd_ == -1) {
        LOG_DEBUG << "Failed to open shared memory: " << shm_name_ << ", error: " << strerror(errno);
        return false;
    }
    
    // 获取共享内存大小
    struct stat shm_stat;
    if (fstat(shm_fd_, &shm_stat) == -1) {
        LOG_DEBUG << "Failed to get shared memory stats: " << strerror(errno);
        close(shm_fd_);
        shm_fd_ = -1;
        return false;
    }
    
    shm_size_ = shm_stat.st_size;
    LOG_DEBUG << "Opened shared memory: " << shm_name_ << ", size: " << shm_size_;
    return true;
}

bool SharedMemoryAccessor::map_shared_memory() {
    shm_addr_ = mmap(nullptr, shm_size_, PROT_READ, MAP_SHARED, shm_fd_, 0);
    if (shm_addr_ == MAP_FAILED) {
        LOG_DEBUG << "Failed to map shared memory: " << strerror(errno);
        shm_addr_ = nullptr;
        return false;
    }
    
    LOG_DEBUG << "Mapped shared memory at address: " << shm_addr_;
    return true;
}

bool SharedMemoryAccessor::validate_magic_numbers() {
    if (!shm_addr_ || shm_size_ < sizeof(DDS::TopicRegistryHeader)) {
        LOG_DEBUG << "Invalid shared memory address or size";
        return false;
    }
    
    DDS::TopicRegistryHeader* header = static_cast<DDS::TopicRegistryHeader*>(shm_addr_);
    
    // 使用正确的魔数值 0x4C444453 ("LDDS")
    if (header->magic_number != 0x4C444453) {
        LOG_DEBUG << "Invalid magic number: expected 0x4C444453, got 0x" 
                  << std::hex << header->magic_number;
        return false;
    }
    
    LOG_DEBUG << "Magic number validation passed";
    return true;
}

void* SharedMemoryAccessor::calculate_ring_buffer_address(size_t offset) {
    if (!shm_addr_ || offset >= shm_size_) {
        return nullptr;
    }
    
    return static_cast<char*>(shm_addr_) + offset;
}

std::string SharedMemoryAccessor::safe_read_string(const char* src, size_t max_len) {
    if (!src) {
        return "";
    }
    
    // 确保字符串以null结尾
    size_t len = 0;
    for (size_t i = 0; i < max_len - 1; ++i) {
        if (src[i] == '\0') {
            len = i;
            break;
        }
        len = i + 1;
    }
    
    return std::string(src, len);
}

} // namespace Monitor
} // namespace MB_DDF