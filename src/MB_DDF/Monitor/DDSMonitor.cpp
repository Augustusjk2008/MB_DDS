/**
 * @file DDSMonitor.cpp
 * @brief DDS系统监控模块实现
 * @date 2025-10-16
 * @author Jiangkai
 */

#include "MB_DDF/Monitor/DDSMonitor.h"
#include "MB_DDF/Monitor/SharedMemoryAccessor.h"
#include "MB_DDF/Debug/Logger.h"
#include <chrono>
#include <sstream>
#include <cstring>

namespace MB_DDF {
namespace Monitor {

DDSMonitor::DDSMonitor(uint32_t scan_interval_ms, uint32_t activity_timeout_ms)
    : scan_interval_ms_(scan_interval_ms), activity_timeout_ms_(activity_timeout_ms),
      dds_core_(nullptr), shm_accessor_(std::make_unique<SharedMemoryAccessor>("/MB_DDF_SHM")),
      monitoring_(false), initialized_(false) {
}

DDSMonitor::~DDSMonitor() {
    stop_monitoring();
}

bool DDSMonitor::initialize(DDS::DDSCore& dds_core) {
    if (initialized_.load()) {
        return true;
    }
    
    dds_core_ = &dds_core;
    
    // 初始化共享内存访问器
    if (!shm_accessor_->connect()) {
        LOG_DEBUG << "Failed to connect to shared memory";
        return false;
    }
    
    initialized_.store(true);
    LOG_DEBUG << "DDSMonitor initialized successfully";
    return true;
}

bool DDSMonitor::start_monitoring() {
    if (!initialized_.load()) {
        LOG_DEBUG << "DDSMonitor not initialized";
        return false;
    }
    
    if (monitoring_.load()) {
        LOG_DEBUG << "DDSMonitor already monitoring";
        return true;
    }
    
    monitoring_.store(true);
    monitor_thread_ = std::thread(&DDSMonitor::monitor_loop, this);
    
    LOG_DEBUG << "DDSMonitor started monitoring with interval " << scan_interval_ms_ << "ms";
    return true;
}

void DDSMonitor::stop_monitoring() {
    if (!monitoring_.load()) {
        return;
    }
    
    monitoring_.store(false);
    
    if (monitor_thread_.joinable()) {
        monitor_thread_.join();
    }
    
    LOG_DEBUG << "DDSMonitor stopped monitoring";
}

DDSSystemSnapshot DDSMonitor::scan_system() {
    DDSSystemSnapshot snapshot;
    snapshot.timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    if (!shm_accessor_ || !shm_accessor_->is_connected()) {
        return snapshot;
    }

    // 获取DDS版本号
    snapshot.dds_version = shm_accessor_->get_memory_layout().registry_header->version;
    
    // 获取所有topic的元数据
    auto topic_metadata_list = shm_accessor_->get_all_topics();
    
    // 统一使用同一时刻的当前时间（纳秒）进行活跃性判断
    const uint64_t now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    for (auto* topic_meta : topic_metadata_list) {
        if (!topic_meta) continue;
        
        TopicInfo topic_info;
        topic_info.topic_id = topic_meta->topic_id;
        topic_info.topic_name = std::string(topic_meta->topic_name);
        topic_info.ring_buffer_size = topic_meta->ring_buffer_size;
        
        // 获取环形缓冲区布局
        auto ring_layout = shm_accessor_->get_ring_buffer_layout(topic_meta);
        
        // 获取环形缓冲区统计信息
        auto ring_stats = shm_accessor_->get_ring_buffer_stats(ring_layout);
        topic_info.total_messages = ring_stats.total_messages;
        topic_info.available_space = ring_stats.available_space;
        
        // 获取发布者信息
        auto publisher_data = shm_accessor_->get_publisher_data(ring_layout);
        topic_info.has_publisher = publisher_data.is_valid;  // 使用实际的发布者检测结果
        
        if (publisher_data.is_valid && ring_layout.header) {
            PublisherInfo pub_info;
            pub_info.publisher_id = publisher_data.publisher_id;
            pub_info.publisher_name = publisher_data.publisher_name;
            pub_info.topic_name = topic_info.topic_name;
            pub_info.topic_id = topic_info.topic_id;
            pub_info.last_sequence = publisher_data.current_sequence;
            // 使用一致的时钟源（steady_clock）读取最新消息时间戳
            const uint64_t last_pub_ts = ring_layout.header->timestamp.load(std::memory_order_acquire);
            pub_info.is_active = is_active(last_pub_ts, now_ns);
            
            snapshot.publishers.push_back(pub_info);
        }
        
        // 获取订阅者信息
        auto subscribers_data = shm_accessor_->get_subscribers_data(ring_layout);
        for (const auto& sub_data : subscribers_data) {
            if (!sub_data.is_valid) continue;
            
            SubscriberInfo sub_info;
            sub_info.subscriber_id = sub_data.subscriber_id;
            sub_info.subscriber_name = sub_data.subscriber_name;
            sub_info.topic_name = topic_info.topic_name;
            sub_info.topic_id = topic_info.topic_id;
            sub_info.read_pos = sub_data.read_pos;
            sub_info.last_read_sequence = sub_data.last_read_sequence;
            sub_info.last_active_time = sub_data.last_active_timestamp;
            sub_info.is_active = is_active(sub_data.last_active_timestamp, now_ns);
            
            snapshot.subscribers.push_back(sub_info);
        }
        
        // 设置实际的订阅者数量
        topic_info.subscriber_count = subscribers_data.size();
        
        snapshot.topics.push_back(topic_info);
    }
    
    // 计算内存使用情况
    auto memory_stats = shm_accessor_->get_memory_usage_stats();
    snapshot.total_shared_memory_size = memory_stats.total_size;
    snapshot.used_shared_memory_size = memory_stats.total_size - memory_stats.free_space;
    
    return snapshot;
}

std::string DDSMonitor::version_to_string(uint32_t version) {
    // 提取各个部分的位
    uint32_t major = (version >> 24) & 0xFF;        // 第25-32位 (高8位)
    uint32_t minor = (version >> 12) & 0xFFF;       // 第13-24位 (中间12位)
    uint32_t patch = version & 0xFFF;               // 第1-12位 (低12位)
        
    return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
};

std::string DDSMonitor::serialize_to_json(const DDSSystemSnapshot& snapshot) {
    std::ostringstream json;
    
    json << "{\n";
    json << "  \"timestamp\": " << snapshot.timestamp << ",\n";
    json << "  \"dds_version\": \"" << version_to_string(snapshot.dds_version) << "\",\n";
    json << "  \"total_shared_memory_size\": " << snapshot.total_shared_memory_size << ",\n";
    json << "  \"used_shared_memory_size\": " << snapshot.used_shared_memory_size << ",\n";
    
    // Topics
    json << "  \"topics\": [\n";
    for (size_t i = 0; i < snapshot.topics.size(); ++i) {
        const auto& topic = snapshot.topics[i];
        json << "    {\n";
        json << "      \"topic_id\": " << topic.topic_id << ",\n";
        json << "      \"topic_name\": \"" << topic.topic_name << "\",\n";
        json << "      \"ring_buffer_size\": " << topic.ring_buffer_size << ",\n";
        json << "      \"has_publisher\": " << (topic.has_publisher ? "true" : "false") << ",\n";
        json << "      \"subscriber_count\": " << topic.subscriber_count << ",\n";
        json << "      \"total_messages\": " << topic.total_messages << ",\n";
        json << "      \"available_space\": " << topic.available_space << "\n";
        json << "    }";
        if (i < snapshot.topics.size() - 1) json << ",";
        json << "\n";
    }
    json << "  ],\n";
    
    // Publishers
    json << "  \"publishers\": [\n";
    for (size_t i = 0; i < snapshot.publishers.size(); ++i) {
        const auto& pub = snapshot.publishers[i];
        json << "    {\n";
        json << "      \"publisher_id\": " << pub.publisher_id << ",\n";
        json << "      \"publisher_name\": \"" << pub.publisher_name << "\",\n";
        json << "      \"topic_name\": \"" << pub.topic_name << "\",\n";
        json << "      \"topic_id\": " << pub.topic_id << ",\n";
        json << "      \"last_sequence\": " << pub.last_sequence << ",\n";
        json << "      \"is_active\": " << (pub.is_active ? "true" : "false") << "\n";
        json << "    }";
        if (i < snapshot.publishers.size() - 1) json << ",";
        json << "\n";
    }
    json << "  ],\n";
    
    // Subscribers
    json << "  \"subscribers\": [\n";
    for (size_t i = 0; i < snapshot.subscribers.size(); ++i) {
        const auto& sub = snapshot.subscribers[i];
        json << "    {\n";
        json << "      \"subscriber_id\": " << sub.subscriber_id << ",\n";
        json << "      \"subscriber_name\": \"" << sub.subscriber_name << "\",\n";
        json << "      \"topic_name\": \"" << sub.topic_name << "\",\n";
        json << "      \"topic_id\": " << sub.topic_id << ",\n";
        json << "      \"read_pos\": " << sub.read_pos << ",\n";
        json << "      \"last_read_sequence\": " << sub.last_read_sequence << ",\n";
        json << "      \"last_active_time\": " << sub.last_active_time << ",\n";
        json << "      \"is_active\": " << (sub.is_active ? "true" : "false") << "\n";
        json << "    }";
        if (i < snapshot.subscribers.size() - 1) json << ",";
        json << "\n";
    }
    json << "  ]\n";
    
    json << "}";
    
    return json.str();
}

size_t DDSMonitor::serialize_to_binary(const DDSSystemSnapshot& snapshot, void* buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) {
        return 0;
    }
    
    char* ptr = static_cast<char*>(buffer);
    size_t offset = 0;
    
    // 简单的二进制序列化格式
    // 格式: [header][topics][publishers][subscribers]
    
    // Header (32 bytes)
    struct BinaryHeader {
        uint64_t timestamp;
        uint32_t dds_version;
        uint64_t total_shared_memory_size;
        uint64_t used_shared_memory_size;
        uint32_t topic_count;
        uint32_t publisher_count;
        uint32_t subscriber_count;
        uint32_t reserved;
    } header;
    
    if (offset + sizeof(header) > buffer_size) return 0;
    
    header.timestamp = snapshot.timestamp;
    header.dds_version = snapshot.dds_version;
    header.total_shared_memory_size = snapshot.total_shared_memory_size;
    header.used_shared_memory_size = snapshot.used_shared_memory_size;
    header.topic_count = static_cast<uint32_t>(snapshot.topics.size());
    header.publisher_count = static_cast<uint32_t>(snapshot.publishers.size());
    header.subscriber_count = static_cast<uint32_t>(snapshot.subscribers.size());
    header.reserved = 0;
    
    std::memcpy(ptr + offset, &header, sizeof(header));
    offset += sizeof(header);
    
    // Topics
    for (const auto& topic : snapshot.topics) {
        struct BinaryTopic {
            uint32_t topic_id;
            char topic_name[64];
            uint64_t ring_buffer_size;
            uint32_t has_publisher;
            uint32_t subscriber_count;
            uint64_t total_messages;
            uint64_t available_space;
        } bin_topic;
        
        if (offset + sizeof(bin_topic) > buffer_size) return 0;
        
        bin_topic.topic_id = topic.topic_id;
        std::strncpy(bin_topic.topic_name, topic.topic_name.c_str(), 63);
        bin_topic.topic_name[63] = '\0';
        bin_topic.ring_buffer_size = topic.ring_buffer_size;
        bin_topic.has_publisher = topic.has_publisher ? 1 : 0;
        bin_topic.subscriber_count = topic.subscriber_count;
        bin_topic.total_messages = topic.total_messages;
        bin_topic.available_space = topic.available_space;
        
        std::memcpy(ptr + offset, &bin_topic, sizeof(bin_topic));
        offset += sizeof(bin_topic);
    }
    
    // Publishers
    for (const auto& pub : snapshot.publishers) {
        struct BinaryPublisher {
            uint64_t publisher_id;
            char publisher_name[64];
            char topic_name[64];
            uint32_t topic_id;
            uint64_t last_sequence;
            uint64_t last_active_time;
            uint32_t is_active;
            uint32_t reserved;
        } bin_pub;
        
        if (offset + sizeof(bin_pub) > buffer_size) return 0;
        
        bin_pub.publisher_id = pub.publisher_id;
        std::strncpy(bin_pub.publisher_name, pub.publisher_name.c_str(), 63);
        bin_pub.publisher_name[63] = '\0';
        std::strncpy(bin_pub.topic_name, pub.topic_name.c_str(), 63);
        bin_pub.topic_name[63] = '\0';
        bin_pub.topic_id = pub.topic_id;
        bin_pub.last_sequence = pub.last_sequence;
        bin_pub.is_active = pub.is_active ? 1 : 0;
        bin_pub.reserved = 0;
        
        std::memcpy(ptr + offset, &bin_pub, sizeof(bin_pub));
        offset += sizeof(bin_pub);
    }
    
    // Subscribers
    for (const auto& sub : snapshot.subscribers) {
        struct BinarySubscriber {
            uint64_t subscriber_id;
            char subscriber_name[64];
            char topic_name[64];
            uint32_t topic_id;
            uint64_t read_pos;
            uint64_t last_read_sequence;
            uint64_t last_active_time;
            uint32_t is_active;
            uint32_t reserved;
        } bin_sub;
        
        if (offset + sizeof(bin_sub) > buffer_size) return 0;
        
        bin_sub.subscriber_id = sub.subscriber_id;
        std::strncpy(bin_sub.subscriber_name, sub.subscriber_name.c_str(), 63);
        bin_sub.subscriber_name[63] = '\0';
        std::strncpy(bin_sub.topic_name, sub.topic_name.c_str(), 63);
        bin_sub.topic_name[63] = '\0';
        bin_sub.topic_id = sub.topic_id;
        bin_sub.read_pos = sub.read_pos;
        bin_sub.last_read_sequence = sub.last_read_sequence;
        bin_sub.last_active_time = sub.last_active_time;
        bin_sub.is_active = sub.is_active ? 1 : 0;
        bin_sub.reserved = 0;
        
        std::memcpy(ptr + offset, &bin_sub, sizeof(bin_sub));
        offset += sizeof(bin_sub);
    }
    
    return offset;
}

void DDSMonitor::set_monitor_callback(std::function<void(const DDSSystemSnapshot&)> callback) {
    monitor_callback_ = callback;
}

DDSSystemSnapshot DDSMonitor::get_latest_snapshot() const {
    std::lock_guard<std::mutex> lock(snapshot_mutex_);
    return latest_snapshot_;
}

void DDSMonitor::monitor_loop() {
    LOG_DEBUG << "DDSMonitor thread started";
    
    while (monitoring_.load()) {
        try {
            // 执行系统扫描
            DDSSystemSnapshot snapshot = scan_system();
            
            // 更新最新快照
            {
                std::lock_guard<std::mutex> lock(snapshot_mutex_);
                latest_snapshot_ = snapshot;
            }
            
            // 调用回调函数
            if (monitor_callback_) {
                monitor_callback_(snapshot);
            }
            
        } catch (const std::exception& e) {
            LOG_DEBUG << "Error in monitor loop: " << e.what();
        }
        
        // 等待下次扫描
        std::this_thread::sleep_for(std::chrono::milliseconds(scan_interval_ms_));
    }
    
    LOG_DEBUG << "DDSMonitor thread stopped";
}

void DDSMonitor::scan_topics(DDSSystemSnapshot& snapshot) {
    // 注意：这里需要访问DDS内部的共享内存和Topic注册表
    // 实际实现中需要DDSCore提供访问接口或友元声明
    
    // 假设我们能够访问到TopicRegistry和SharedMemoryManager
    // 这里提供一个示例实现框架
    
    /*
    // 获取所有Topic
    auto topics = topic_registry_->get_all_topics();
    
    for (auto* topic_metadata : topics) {
        if (!topic_metadata) continue;
        
        TopicInfo topic_info;
        topic_info.topic_id = topic_metadata->topic_id;
        topic_info.topic_name = std::string(topic_metadata->topic_name);
        topic_info.ring_buffer_size = topic_metadata->ring_buffer_size;
        topic_info.has_publisher = topic_metadata->has_publisher.load();
        topic_info.subscriber_count = topic_metadata->subscriber_count.load();
        
        // 获取RingBuffer统计信息
        RingBuffer* ring_buffer = get_ring_buffer_for_topic(topic_metadata);
        if (ring_buffer) {
            auto stats = ring_buffer->get_statistics();
            topic_info.total_messages = stats.total_messages;
            topic_info.available_space = stats.available_space;
            
            // 扫描该Topic的参与者
            scan_topic_participants(topic_metadata, ring_buffer, snapshot);
        }
        
        snapshot.topics.push_back(topic_info);
    }
    */
    
    // 临时实现：创建一些示例数据用于测试
    TopicInfo example_topic;
    example_topic.topic_id = 1;
    example_topic.topic_name = "example_topic";
    example_topic.ring_buffer_size = 1024 * 1024;
    example_topic.has_publisher = true;
    example_topic.subscriber_count = 2;
    example_topic.total_messages = 100;
    example_topic.available_space = 512 * 1024;
    
    snapshot.topics.push_back(example_topic);
}

bool DDSMonitor::is_active(uint64_t timestamp, uint64_t current_time) const {
    if (timestamp == 0) return false;
    
    uint64_t timeout_ns = static_cast<uint64_t>(activity_timeout_ms_) * 1000000ULL;
    return (current_time - timestamp) <= timeout_ns;
}

uint64_t DDSMonitor::get_current_timestamp() const {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
}

void DDSMonitor::calculate_memory_usage(DDSSystemSnapshot& snapshot) {
    // 计算共享内存使用情况
    // 这需要访问SharedMemoryManager的内部信息
    
    /*
    if (shm_manager_) {
        snapshot.total_shared_memory_size = shm_manager_->get_size();
        
        // 计算已使用的内存
        size_t used_size = 0;
        for (const auto& topic : snapshot.topics) {
            used_size += topic.ring_buffer_size;
        }
        used_size += sizeof(DDS::TopicRegistryHeader);
        used_size += snapshot.topics.size() * sizeof(DDS::TopicMetadata);
        
        snapshot.used_shared_memory_size = used_size;
    }
    */
    
    // 临时实现
    snapshot.total_shared_memory_size = 128 * 1024 * 1024; // 128MB
    snapshot.used_shared_memory_size = 64 * 1024 * 1024;   // 64MB
}

} // namespace Monitor
} // namespace MB_DDF