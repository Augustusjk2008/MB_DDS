/**
 * @file TopicRegistry.h
 * @brief Topic注册表管理类定义
 * @date 2025-08-03
 * @author Jiangkai
 * 
 * 提供Topic的注册、查询和管理功能。
 * 维护Topic元数据信息，支持多进程间的Topic发现和管理。
 */

#pragma once

#include <cstdint>
#include <string>
#include <atomic>
#include <vector>

namespace MB_DDF {
namespace DDS {

/**
 * @struct TopicMetadata
 * @brief Topic元数据结构
 * 
 * 存储Topic的基本信息、环形缓冲区位置以及统计信息。
 * 使用原子操作确保多进程访问的线程安全性。
 */
struct alignas(64) TopicMetadata {
    uint32_t topic_id;                          ///< Topic的唯一标识符
    char topic_name[64];                        ///< Topic名称（最大63字符 + 终止符）
    size_t ring_buffer_offset;                  ///< 环形缓冲区在共享内存中的偏移量
    size_t ring_buffer_size;                    ///< 环形缓冲区的大小（字节）
    std::atomic<bool> has_publisher;            ///< 是否已有发布者（只允许一个发布者）
    std::atomic<uint32_t> subscriber_count;     ///< 当前订阅者数量
    std::atomic<uint64_t> last_sequence;        ///< 最后发布的消息序列号

    /**
     * @brief 默认构造函数，初始化所有字段
     */
    TopicMetadata() : topic_id(0), ring_buffer_offset(0), ring_buffer_size(0),
                      has_publisher(false), subscriber_count(0), last_sequence(0) {
        topic_name[0] = '\0';
    }
};

/**
 * @struct TopicRegistryHeader
 * @brief 共享内存中的Topic注册表头部结构
 * 
 * 存储注册表的全局状态信息，包括Topic计数和ID分配。
 */
struct alignas(64) TopicRegistryHeader {
    uint32_t magic_number;                  ///< 魔数标记，用于验证共享内存是否已正确初始化
    std::atomic<uint32_t> next_topic_id;   ///< 下一个可用的Topic ID
    std::atomic<uint32_t> topic_count;     ///< 当前注册的Topic数量
    // 可扩展其他全局元数据
};

/**
 * @class TopicRegistry
 * @brief Topic注册表管理类
 * 
 * 负责管理所有Topic的注册、查询和元数据维护。
 * 基于共享内存实现，支持多进程间的Topic发现和管理。
 */
class TopicRegistry {
public:
    /**
     * @brief 构造函数
     * @param shm_base_addr 共享内存基地址
     * @param shm_size 共享内存总大小
     * @param shm_manager 共享内存管理器指针（用于获取信号量）
     */
    TopicRegistry(void* shm_base_addr, size_t shm_size, class SharedMemoryManager* shm_manager);

    /**
     * @brief 注册新的Topic
     * @param name Topic名称
     * @param rb_size 环形缓冲区大小
     * @return 成功返回Topic元数据指针，失败返回nullptr
     */
    TopicMetadata* register_topic(const std::string& name, size_t rb_size);
    
    /**
     * @brief 根据名称获取Topic元数据
     * @param name Topic名称
     * @return Topic元数据指针，不存在时返回nullptr
     */
    TopicMetadata* get_topic_metadata(const std::string& name);
    
    /**
     * @brief 根据ID获取Topic元数据
     * @param topic_id Topic ID
     * @return Topic元数据指针，不存在时返回nullptr
     */
    TopicMetadata* get_topic_metadata(uint32_t topic_id);

    /**
     * @brief 获取所有已注册的Topic列表
     * @return Topic元数据指针向量
     */
    std::vector<TopicMetadata*> get_all_topics();

    /**
     * @brief 验证Topic名称格式是否合法
     * @param name Topic名称
     * @return 格式合法返回true，否则返回false
     * 
     * 要求格式为"[域名称]://[地址名称]"，域名称和地址名称均不得为空
     */
    bool is_valid_topic_name(const std::string& name);

private:
    void* shm_base_addr_;                ///< 共享内存基地址
    size_t shm_size_;                    ///< 共享内存总大小
    class SharedMemoryManager* shm_manager_; ///< 共享内存管理器指针
    TopicRegistryHeader* header_;        ///< 注册表头部指针
    TopicMetadata* metadata_array_;      ///< Topic元数据数组指针

    static constexpr size_t MAX_TOPICS = 128;                                           ///< 最大支持的Topic数量
    static constexpr size_t METADATA_OFFSET = sizeof(TopicRegistryHeader);             ///< 元数据数组在共享内存中的偏移
    static constexpr size_t DATA_OFFSET = METADATA_OFFSET + MAX_TOPICS * sizeof(TopicMetadata); ///< 环形缓冲区数据区偏移
    static constexpr size_t ALIGNMENT = 64;                                             ///< 内存对齐大小
};

} // namespace DDS
} // namespace MB_DDF


