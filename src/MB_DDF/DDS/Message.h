/**
 * @file Message.h
 * @brief 消息结构定义和相关数据类型
 * @date 2025-08-03
 * @author Jiangkai
 * 
 * 定义了DDS系统中使用的消息格式、消息头、Topic信息等核心数据结构。
 * 包含消息的序列化、校验和时间戳等功能。
 */

#pragma once

#include <cstdint>
#include <chrono>
#include <nmmintrin.h>

namespace MB_DDF {
namespace DDS {

// 初始化CRC32表
static uint32_t crc32_table[256];
static bool table_initialized = false;

/**
 * @struct MessageHeader
 * @brief 消息头结构，包含消息的元数据信息
 * 
 * 每个消息都以此结构开头，用于消息的验证、路由和时序管理。
 */
struct alignas(8) MessageHeader {
    uint32_t magic;           ///< 魔数，用于消息格式验证
    uint32_t topic_id;        ///< Topic唯一标识符
    uint64_t sequence;        ///< 消息序列号，用于排序和去重
    uint64_t timestamp;       ///< 消息时间戳（纳秒精度）
    uint32_t data_size;       ///< 消息数据部分的大小（字节）
    uint32_t checksum;        ///< 消息校验和，用于数据完整性验证
    
    static constexpr uint32_t MAGIC_NUMBER = 0xDEADBEEF; ///< 消息魔数常量
    
    /**
     * @brief 默认构造函数，初始化所有字段
     */
    MessageHeader() : magic(MAGIC_NUMBER), topic_id(0), sequence(0), 
                     timestamp(0), data_size(0), checksum(0) {}
    
    /**
     * @brief 设置当前时间戳
     * 使用steady时钟获取当前时间并设置到timestamp字段
     */
    void set_timestamp() {
        auto now = std::chrono::steady_clock::now();
        timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
            now.time_since_epoch()).count();
    }
    
    /**
     * @brief 验证消息头的有效性
     * @return 消息头有效返回true，否则返回false
     */
    bool is_valid() const {
        return magic == MAGIC_NUMBER;
    }
    
    /**
     * @brief 初始化CRC32表
     * 
     * 用于计算校验和时的查表加速。
     * 确保在首次使用校验和计算前调用。
     */
    static void initialize_crc32_table() {
        if (table_initialized) return;
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t crc = i;
            for (int j = 0; j < 8; ++j) {  // 预计算每个字节的8位处理结果
                if (crc & 1) {
                    crc = (crc >> 1) ^ 0xEDB88320;
                } else {
                    crc >>= 1;
                }
            }
            crc32_table[i] = crc;
        }
        table_initialized = true;
    }
    
    /**
     * @brief 计算数据的校验
     * @param data 数据指针
     * @param size 数据大小
     * @return 计算得到的校验
     */
    static uint32_t calculate_checksum(const void* data, size_t size) {
        // 空指针或零大小数据返回0
        if (data == nullptr || size == 0) {
            return 0;
        }

        // 校验和计算采用CRC32算法，未考虑字节序
        initialize_crc32_table();  // 确保表已初始化
        const uint8_t* bytes = static_cast<const uint8_t*>(data);
        uint32_t crc = 0xFFFFFFFF;

        for (size_t i = 0; i < size; ++i) {
            // 用当前CRC的低8位查表，再更新CRC
            crc = (crc >> 8) ^ crc32_table[(crc & 0xFF) ^ bytes[i]];
        }

        return ~crc;
    }
    
    /**
     * @brief 设置校验和
     * @param data 数据指针
     * @param size 数据大小
     */
    void set_checksum(const void* data, size_t size) {
        checksum = calculate_checksum(data, size);
    }
    
    /**
     * @brief 验证校验和
     * @param data 数据指针
     * @param size 数据大小
     * @return 校验和正确返回true，否则返回false
     */
    bool verify_checksum(const void* data, size_t size) const {
        return checksum == calculate_checksum(data, size);
    }
};

/**
 * @struct Message
 * @brief 完整的消息结构
 * 
 * 采用头数据分离的设计。
 * 适用于共享内存场景，避免大块数据的拷贝操作。
 */
struct alignas(8) Message {
    MessageHeader header;     ///< 消息头
    
    /**
     * @brief 默认构造函数
     */
    Message() {}
    
    /**
     * @brief 构造函数，初始化消息头和数据指针
     * @param topic_id Topic ID
     * @param sequence 序列号
     * @param data 数据指针
     * @param data_size 数据大小
     */
    Message(uint32_t topic_id, uint64_t sequence, void* data, uint32_t data_size) {
        header.topic_id = topic_id;
        header.sequence = sequence;
        header.data_size = data_size;
        header.set_timestamp();
        if (data && data_size > 0) {
            header.set_checksum(data, data_size);
        }
    }
    
    /**
     * @brief 计算包含指定数据大小的消息总大小
     * @param data_size 数据部分的大小
     * @return 消息总大小（消息头 + 数据部分）
     */
    static size_t total_size(size_t data_size) {
        return sizeof(MessageHeader) + data_size;
    }
    
    /**
     * @brief 计算当前消息的总大小（消息头 + 数据部分）
     * @return 消息总大小
     */
    size_t msg_size() const {
        return sizeof(MessageHeader) + header.data_size;
    }

    /**
     * @brief 获取数据部分的大小
     * @return 数据部分大小（字节）
     */
    size_t msg_data_size() const {
        return header.data_size;
    }
    
    /**
     * @brief 获取数据部分的可写指针
     * @return 数据部分指针
     */
    void* get_data() { return reinterpret_cast<char*>(this) + sizeof(Message); }
    
    /**
     * @brief 获取数据部分的只读指针
     * @return 数据部分常量指针
     */
    const void* get_data() const { return reinterpret_cast<const char*>(this) + sizeof(Message); }
    
    /**
     * @brief 验证消息的完整性
     * @return 消息有效且校验和正确返回true，否则返回false
     */
    bool is_valid() const {
        if (!header.is_valid()) {
            return false;
        }
        if (header.data_size > 0) {
            return header.verify_checksum(get_data(), header.data_size);
        }
        return true; // 空数据消息也是有效的
    }
    
    /**
     * @brief 更新消息的时间戳和校验和
     */
    void update() {
        header.set_timestamp();
        header.set_checksum(get_data(), header.data_size);
    }
};

} // namespace DDS
} // namespace MB_DDF


