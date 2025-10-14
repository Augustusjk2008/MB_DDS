/**
 * @file SharedMemory.cpp
 * @brief 共享内存管理类实现
 * @date 2025-08-03
 * @author Jiangkai
 * 
 * 实现跨进程共享内存的创建、映射和管理功能。
 * 基于POSIX共享内存API，支持多进程安全访问。
 */

#include "SharedMemory.h"
#include <iostream>
#include <stdexcept>
#include <cstring> // For strerror
#include <sys/stat.h> // For fstat

namespace MB_DDS {
namespace Core {

SharedMemoryManager::SharedMemoryManager(const std::string& name, size_t size)
    : shm_name_(name), shm_size_(size), shm_fd_(-1), shm_addr_(nullptr), shm_sem_(nullptr) {
    if (!create_or_open_shm()) {
        return; // 构造失败，对象处于无效状态
    }
    if (!map_shm()) {
        return; // 构造失败，对象处于无效状态
    }
    if (!create_or_open_semaphore()) {
        return; // 构造失败，对象处于无效状态
    }
}

SharedMemoryManager::~SharedMemoryManager() {
    if (shm_addr_ != MAP_FAILED && shm_addr_ != nullptr) {
        if (munmap(shm_addr_, shm_size_) == -1) {
            std::cerr << "Error unmapping shared memory: " << strerror(errno) << std::endl;
        }
    }
    if (shm_fd_ != -1) {
        if (close(shm_fd_) == -1) {
            std::cerr << "Error closing shared memory file descriptor: " << strerror(errno) << std::endl;
        }
    }
    if (shm_sem_ != SEM_FAILED && shm_sem_ != nullptr) {
        if (sem_close(shm_sem_) == -1) {
            std::cerr << "Error closing semaphore: " << strerror(errno) << std::endl;
        }
    }
}

bool SharedMemoryManager::create_or_open_shm() {
    shm_fd_ = shm_open(shm_name_.c_str(), O_CREAT | O_RDWR, 0666);
    if (shm_fd_ == -1) {
        std::cerr << "shm_open failed: " << strerror(errno) << std::endl;
        return false;
    }
    // 检查并调整共享内存大小
    struct stat sb;
    if (fstat(shm_fd_, &sb) == -1) {
        std::cerr << "fstat failed: " << strerror(errno) << std::endl;
        close(shm_fd_);
        shm_fd_ = -1;  // 重置文件描述符，避免资源泄漏
        return false;
    }
    if (sb.st_size == 0) { // 新创建或截断的共享内存
        if (ftruncate(shm_fd_, shm_size_) == -1) {
            std::cerr << "ftruncate failed: " << strerror(errno) << std::endl;
            close(shm_fd_);
            shm_fd_ = -1;  // 重置文件描述符，避免资源泄漏
            return false;
        }
    } else if ((size_t)sb.st_size != shm_size_) {
        std::cerr << "Warning: Shared memory segment \"" << shm_name_ 
                  << "\" already exists with different size. Expected " 
                  << shm_size_ << ", got " << sb.st_size << std::endl;
        // 大小不匹配时，根据需求决定是否调整大小或失败
        // 这里选择失败，避免数据不一致
        close(shm_fd_);
        shm_fd_ = -1;  // 重置文件描述符，避免资源泄漏
        return false;
    }
    return true;
}

bool SharedMemoryManager::map_shm() {
    // 添加MAP_POPULATE标志以预分配物理内存，避免嵌入式平台上的页面错误
    shm_addr_ = mmap(nullptr, shm_size_, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, shm_fd_, 0);
    if (shm_addr_ == MAP_FAILED) {
        std::cerr << "mmap failed: " << strerror(errno) << std::endl;
        close(shm_fd_);
        shm_fd_ = -1;  // 重置文件描述符，避免资源泄漏
        return false;
    }
    return true;
}

bool SharedMemoryManager::create_or_open_semaphore() {
    // 信号量名称应唯一，通常基于共享内存名称
    std::string sem_name = shm_name_ + "_sem";
    shm_sem_ = sem_open(sem_name.c_str(), O_CREAT, 0666, 1); // 初始值1，模拟互斥锁行为
    if (shm_sem_ == SEM_FAILED) {
        std::cerr << "sem_open failed: " << strerror(errno) << std::endl;
        return false;
    }
    return true;
}

} // namespace Core
} // namespace MB_DDS


