/**
 * @file SharedMemory.cpp
 * @brief 共享内存管理类实现
 * @date 2025-08-03
 * @author Jiangkai
 * 
 * 实现跨进程共享内存的创建、映射和管理功能。
 * 基于POSIX共享内存API，支持多进程安全访问。
 */

#include "MB_DDF/DDS/SharedMemory.h"
#include "MB_DDF/Debug/Logger.h"
#include <cstring> // For strerror
#include <sys/stat.h> // For fstat
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>

namespace MB_DDF {
namespace DDS {

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
            LOG_ERROR << "Error unmapping shared memory: " << strerror(errno);
        }
    }
    if (shm_fd_ != -1) {
        if (close(shm_fd_) == -1) {
            LOG_ERROR << "Error closing shared memory file descriptor: " << strerror(errno);
        }
    }
    if (shm_sem_ != SEM_FAILED && shm_sem_ != nullptr) {
        if (sem_close(shm_sem_) == -1) {
            LOG_ERROR << "Error closing semaphore: " << strerror(errno);
        }
    }
}

bool SharedMemoryManager::create_or_open_shm() {
    shm_fd_ = shm_open(shm_name_.c_str(), O_CREAT | O_RDWR, 0666);
    if (shm_fd_ == -1) {
        LOG_ERROR << "shm_open failed: " << strerror(errno);
        return false;
    }
    // 检查并调整共享内存大小
    struct stat sb;
    if (fstat(shm_fd_, &sb) == -1) {
        LOG_ERROR << "fstat failed: " << strerror(errno);
        close(shm_fd_);
        shm_fd_ = -1;  // 重置文件描述符，避免资源泄漏
        return false;
    }
    if (sb.st_size == 0) { // 新创建或截断的共享内存
        if (ftruncate(shm_fd_, shm_size_) == -1) {
            LOG_ERROR << "ftruncate failed: " << strerror(errno);
            close(shm_fd_);
            shm_fd_ = -1;  // 重置文件描述符，避免资源泄漏
            return false;
        }
    } else if ((size_t)sb.st_size != shm_size_) {
        LOG_ERROR << "Warning: Shared memory segment \"" << shm_name_ 
                  << "\" already exists with different size. Expected " 
                  << shm_size_ << ", got " << sb.st_size;
        // 大小不匹配时，根据需求决定是否调整大小或失败
        // 这里选择失败，避免数据不一致
        close(shm_fd_);
        shm_fd_ = -1;  // 重置文件描述符，避免资源泄漏
        return false;
    }

    LOG_DEBUG << "Shared memory segment \"" << shm_name_ << "\" created or opened with size " << shm_size_;
    return true;
}

bool SharedMemoryManager::map_shm() {
    // 添加MAP_POPULATE标志以预分配物理内存，避免嵌入式平台上的页面错误
    shm_addr_ = mmap(nullptr, shm_size_, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, shm_fd_, 0);
    if (shm_addr_ == MAP_FAILED) {
        LOG_ERROR << "mmap failed: " << strerror(errno);
        close(shm_fd_);
        shm_fd_ = -1;  // 重置文件描述符，避免资源泄漏
        return false;
    }

    LOG_DEBUG << "Shared memory segment \"" << shm_name_ << "\" mapped to address " << shm_addr_;
    return true;
}

bool SharedMemoryManager::create_or_open_semaphore() {
    // 信号量名称应唯一，通常基于共享内存名称
    std::string sem_name = shm_name_ + "_sem";
    shm_sem_ = sem_open(sem_name.c_str(), O_CREAT, 0666, 1); // 初始值1，模拟互斥锁行为
    if (shm_sem_ == SEM_FAILED) {
        LOG_ERROR << "sem_open failed: " << strerror(errno);
        return false;
    }

    // 检测信号量状态并处理潜在死锁
    int sem_value;
    if (sem_getvalue(shm_sem_, &sem_value) == -1) {
        LOG_ERROR << "sem_getvalue failed: " << strerror(errno);
        return false;
    }

    // 如果信号量被锁定（值为0），检查是否死锁
    if (sem_value == 0) {
        // 使用文件锁确保恢复操作的原子性
        std::string lock_path = "/tmp/" + sem_name + ".lock";
        int lock_fd = open(lock_path.c_str(), O_CREAT | O_RDWR, 0666);
        if (lock_fd == -1) {
            LOG_ERROR << "Failed to create lock file: " << strerror(errno);
            return false;
        }

        // 获取独占锁，防止多个进程同时执行恢复操作
        if (flock(lock_fd, LOCK_EX) == -1) {
            LOG_ERROR << "Failed to acquire file lock: " << strerror(errno);
            close(lock_fd);
            return false;
        }

        // 再次检查信号量状态（可能已被其他进程恢复）
        sem_getvalue(shm_sem_, &sem_value);
        if (sem_value == 0) {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_nsec += 20 * 1000000; // 20ms超时
            
            // 处理纳秒溢出
            if (ts.tv_nsec >= 1000000000) {
                ts.tv_sec += 1;
                ts.tv_nsec -= 1000000000;
            }

            // 尝试带超时的等待
            if (sem_timedwait(shm_sem_, &ts) == -1) {
                if (errno == ETIMEDOUT) {
                    LOG_WARN << "Semaphore appears deadlocked. Resetting...";
                    
                    // 强制释放信号量
                    if (sem_post(shm_sem_) == -1) {
                        LOG_ERROR << "Failed to reset semaphore: " << strerror(errno);
                    } else {
                        LOG_INFO << "Semaphore reset successfully";
                    }
                } else {
                    LOG_ERROR << "sem_timedwait error: " << strerror(errno);
                }
            } else {
                // 成功获取信号量，立即释放
                sem_post(shm_sem_);
            }
        }

        // 释放文件锁并清理
        flock(lock_fd, LOCK_UN);
        close(lock_fd);
        unlink(lock_path.c_str());
    }

    LOG_DEBUG << "Semaphore \"" << sem_name << "\" created or opened";
    return true;
}

} // namespace DDS
} // namespace MB_DDF


