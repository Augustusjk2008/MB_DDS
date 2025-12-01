#pragma once
#include <string>
#include <cstring>
#include <cstdio>
#include <cstdint>

namespace MB_DDF {
namespace Tools {

class MD5 {
private:
    struct md5_context {
        uint32_t total[2];     /*!< number of bytes processed  */
        uint32_t state[4];     /*!< intermediate digest state  */
        unsigned char buffer[64];   /*!< data block being processed */
    };
    
    md5_context ctx;
    
    /*
     * 现代C++实现的32位整数操作（小端序）
     */
    
    // 检测系统字节序
    static constexpr bool is_little_endian();
    
    // 现代C++版本的GET_ULONG_LE
    static inline uint32_t get_ulong_le(const unsigned char* data, size_t offset);
    
    // 现代C++版本的PUT_ULONG_LE
    static inline void put_ulong_le(uint32_t value, unsigned char* data, size_t offset);
    
    void md5_process(const unsigned char data[64]);
    
    static const unsigned char md5_padding[64];
    
public:
    /**
     * \brief MD5构造函数，初始化MD5上下文
     */
    MD5();
    
    /**
     * \brief 重置MD5上下文
     */
    void reset();
    
    /**
     * \brief 更新MD5哈希值
     * \param input 输入数据缓冲区
     * \param ilen 输入数据长度
     */
    void update(const unsigned char* input, int ilen);
    
    /**
     * \brief 更新MD5哈希值（字符串版本）
     * \param input 输入字符串
     */
    void update(const std::string& input);
    
    /**
     * \brief 完成MD5计算并获取结果
     * \param output 输出缓冲区（16字节）
     */
    void finalize(unsigned char output[16]);
    
    /**
     * \brief 计算字符串的MD5哈希值
     * \param input 输入字符串
     * \return MD5哈希值的十六进制字符串
     */
    std::string hash(const std::string& input);
    
    /**
     * \brief 计算数据的MD5哈希值
     * \param input 输入数据
     * \param ilen 数据长度
     * \param output 输出缓冲区（16字节）
     */
    static void hash(const unsigned char* input, int ilen, unsigned char output[16]);
};

} // namespace Tools
} // namespace MB_DDF
