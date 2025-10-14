/**
 * @file SharedMemory.h
 * @brief 共享内存管理类定义
 * @date 2025-08-03
 * @author Jiangkai
 * 
 * 提供跨进程共享内存的创建、映射和管理功能。
 * 基于POSIX共享内存API实现，支持多进程安全访问。
 */

#pragma once

#include <string>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>

namespace MB_DDS {
namespace Core {

/**
 * @class SharedMemoryManager
 * @brief 共享内存管理器类
 * 
 * 负责创建、映射和管理进程间共享内存区域。
 * 提供线程安全的共享内存访问接口，支持多进程并发访问。
 */
class SharedMemoryManager {
public:
    /**
     * @brief 构造函数，创建或打开共享内存
     * @param name 共享内存名称
     * @param size 共享内存大小
     * @throws std::runtime_error 创建或映射失败时抛出异常
     */
    SharedMemoryManager(const std::string& name, size_t size);
    
    /**
     * @brief 析构函数，清理共享内存资源
     */
    ~SharedMemoryManager();

    /**
     * @brief 获取共享内存地址
     * @return 共享内存起始地址
     */
    void* get_address() const { return shm_addr_; }
    
    /**
     * @brief 获取共享内存大小
     * @return 共享内存大小（字节）
     */
    size_t get_size() const { return shm_size_; }
    
    /**
     * @brief 获取共享内存访问信号量
     * @return 信号量指针
     */
    sem_t* get_semaphore() const { return shm_sem_; }

private:
    std::string shm_name_;    ///< 共享内存名称
    size_t shm_size_;         ///< 共享内存大小
    int shm_fd_;              ///< 共享内存文件描述符
    void* shm_addr_;          ///< 共享内存映射地址
    sem_t* shm_sem_;          ///< 共享内存访问信号量

    /**
     * @brief 创建或打开共享内存对象
     * @return 成功返回true，失败返回false
     */
    bool create_or_open_shm();
    
    /**
     * @brief 映射共享内存到进程地址空间
     * @return 成功返回true，失败返回false
     */
    bool map_shm();
    
    /**
     * @brief 创建或打开共享内存访问信号量
     * @return 成功返回true，失败返回false
     */
    bool create_or_open_semaphore();
};

} // namespace Core
} // namespace MB_DDS


