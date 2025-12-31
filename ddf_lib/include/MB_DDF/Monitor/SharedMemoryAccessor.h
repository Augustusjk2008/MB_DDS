/**
 * @file SharedMemoryAccessor.h
 * @brief 共享内存直接访问器
 * @date 2025-10-16
 * @author Jiangkai
 * 
 * 提供直接从共享内存读取DDS系统数据的功能，
 * 不依赖程序中的变量，确保监控数据的独立性和准确性。
 */

#pragma once

#include "MB_DDF/DDS/TopicRegistry.h"
#include "MB_DDF/DDS/RingBuffer.h"
#include <string>
#include <vector>

namespace MB_DDF {
namespace Monitor {

/**
 * @struct SharedMemoryLayout
 * @brief 共享内存布局信息
 */
struct SharedMemoryLayout {
    void* base_address;                     ///< 共享内存基地址
    size_t total_size;                      ///< 总大小
    DDS::TopicRegistryHeader* registry_header; ///< Topic注册表头部
    DDS::TopicMetadata* topics_array;       ///< Topic元数据数组
    size_t data_area_offset;                ///< 数据区偏移
    
    SharedMemoryLayout() : base_address(nullptr), total_size(0), 
                          registry_header(nullptr), topics_array(nullptr), 
                          data_area_offset(0) {}
};

/**
 * @struct RingBufferLayout
 * @brief 环形缓冲区布局信息
 */
struct RingBufferLayout {
    void* buffer_base;                      ///< 缓冲区基地址
    size_t buffer_size;                     ///< 缓冲区大小
    DDS::RingHeader* header;                ///< 环形缓冲区头部
    void* subscriber_registry;              ///< 订阅者注册表（使用void*避免访问私有类型）
    void* data_area;                        ///< 数据区地址
    size_t data_capacity;                   ///< 数据区容量
    
    RingBufferLayout() : buffer_base(nullptr), buffer_size(0), 
                        header(nullptr), subscriber_registry(nullptr), 
                        data_area(nullptr), data_capacity(0) {}
};

/**
 * @class SharedMemoryAccessor
 * @brief 共享内存直接访问器类
 * 
 * 提供直接从共享内存读取DDS系统数据的功能，
 * 绕过程序变量，确保监控数据的准确性和实时性。
 */
class SharedMemoryAccessor {
public:
    /**
     * @brief 构造函数
     * @param shm_name 共享内存名称
     */
    explicit SharedMemoryAccessor(const std::string& shm_name = "/dds_shared_memory");
    
    /**
     * @brief 析构函数
     */
    ~SharedMemoryAccessor();

    /**
     * @brief 连接到共享内存
     * @return 连接成功返回true，失败返回false
     */
    bool connect();
    
    /**
     * @brief 断开共享内存连接
     */
    void disconnect();
    
    /**
     * @brief 检查是否已连接
     * @return 已连接返回true，否则返回false
     */
    bool is_connected() const { return connected_; }
    
    /**
     * @brief 解析共享内存布局
     * @return 解析成功返回true，失败返回false
     */
    bool parse_memory_layout();
    
    /**
     * @brief 获取共享内存布局信息
     * @return 共享内存布局结构
     */
    const SharedMemoryLayout& get_memory_layout() const { return memory_layout_; }
    
    /**
     * @brief 获取所有有效的Topic元数据
     * @return Topic元数据指针向量
     */
    std::vector<DDS::TopicMetadata*> get_all_topics();
    
    /**
     * @brief 根据Topic元数据获取对应的环形缓冲区布局
     * @param topic_metadata Topic元数据指针
     * @return 环形缓冲区布局信息
     */
    RingBufferLayout get_ring_buffer_layout(DDS::TopicMetadata* topic_metadata);
    
    /**
     * @brief 获取环形缓冲区的发布者信息
     * @param ring_layout 环形缓冲区布局
     * @return 发布者信息，如果没有发布者返回空结构
     */
    struct PublisherData {
        uint64_t publisher_id;
        std::string publisher_name;
        uint64_t current_sequence;
        bool is_valid;
        
        PublisherData() : publisher_id(0), current_sequence(0), is_valid(false) {}
    };
    PublisherData get_publisher_data(const RingBufferLayout& ring_layout);
    
    /**
     * @brief 获取环形缓冲区的所有订阅者信息
     * @param ring_layout 环形缓冲区布局
     * @return 订阅者信息向量
     */
    struct SubscriberData {
        uint64_t subscriber_id;
        std::string subscriber_name;
        uint64_t read_pos;
        uint64_t last_read_sequence;
        uint64_t last_active_timestamp;
        bool is_valid;
        
        SubscriberData() : subscriber_id(0), read_pos(0), 
                          last_read_sequence(0), last_active_timestamp(0), 
                          is_valid(false) {}
    };
    std::vector<SubscriberData> get_subscribers_data(const RingBufferLayout& ring_layout);
    
    /**
     * @brief 获取环形缓冲区统计信息
     * @param ring_layout 环形缓冲区布局
     * @return 统计信息结构
     */
    struct RingBufferStats {
        uint64_t total_messages;
        size_t available_space;
        size_t used_space;
        uint32_t active_subscribers;
        
        RingBufferStats() : total_messages(0), available_space(0), 
                           used_space(0), active_subscribers(0) {}
    };
    RingBufferStats get_ring_buffer_stats(const RingBufferLayout& ring_layout);
    
    /**
     * @brief 验证共享内存数据的完整性
     * @return 数据完整返回true，否则返回false
     */
    bool validate_memory_integrity();
    
    /**
     * @brief 获取共享内存使用统计
     * @return 使用统计信息
     */
    struct MemoryUsageStats {
        size_t total_size;
        size_t registry_size;
        size_t topics_metadata_size;
        size_t ring_buffers_size;
        size_t free_space;
        
        MemoryUsageStats() : total_size(0), registry_size(0), 
                            topics_metadata_size(0), ring_buffers_size(0), 
                            free_space(0) {}
    };
    MemoryUsageStats get_memory_usage_stats();

private:
    std::string shm_name_;                  ///< 共享内存名称
    int shm_fd_;                            ///< 共享内存文件描述符
    void* shm_addr_;                        ///< 共享内存映射地址
    size_t shm_size_;                       ///< 共享内存大小
    bool connected_;                        ///< 连接状态
    
    SharedMemoryLayout memory_layout_;      ///< 共享内存布局信息
    
    /**
     * @brief 打开共享内存
     * @return 成功返回true，失败返回false
     */
    bool open_shared_memory();
    
    /**
     * @brief 映射共享内存
     * @return 成功返回true，失败返回false
     */
    bool map_shared_memory();
    
    /**
     * @brief 验证共享内存魔数
     * @return 验证通过返回true，否则返回false
     */
    bool validate_magic_numbers();
    
    /**
     * @brief 计算环形缓冲区在共享内存中的地址
     * @param offset 偏移量
     * @return 计算得到的地址
     */
    void* calculate_ring_buffer_address(size_t offset);
    
    /**
     * @brief 安全地读取字符串
     * @param src 源地址
     * @param max_len 最大长度
     * @return 读取到的字符串
     */
    std::string safe_read_string(const char* src, size_t max_len);
};

} // namespace Monitor
} // namespace MB_DDF