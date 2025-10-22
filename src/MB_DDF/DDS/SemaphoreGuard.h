/**
 * @file SemaphoreGuard.h
 * @brief 基于RAII的信号量守护对象
 */
#pragma once

#include <semaphore.h>
#include <cerrno>
#include <cstring>
#include "MB_DDF/Debug/Logger.h"

namespace MB_DDF {
namespace DDS {

class SemaphoreGuard {
public:
    explicit SemaphoreGuard(sem_t* sem) : sem_(sem), acquired_(false) {
        if (sem_ == nullptr) {
            LOG_ERROR << "SemaphoreGuard construct failed: sem is nullptr";
            return;
        }
        if (sem_wait(sem_) == 0) {
            acquired_ = true;
        } else {
            LOG_ERROR << "sem_wait failed: " << strerror(errno);
        }
    }

    ~SemaphoreGuard() {
        if (acquired_) {
            if (sem_post(sem_) != 0) {
                LOG_ERROR << "sem_post failed: " << strerror(errno);
            }
        }
    }

    SemaphoreGuard(const SemaphoreGuard&) = delete;
    SemaphoreGuard& operator=(const SemaphoreGuard&) = delete;

    SemaphoreGuard(SemaphoreGuard&& other) noexcept : sem_(other.sem_), acquired_(other.acquired_) {
        other.sem_ = nullptr;
        other.acquired_ = false;
    }
    SemaphoreGuard& operator=(SemaphoreGuard&& other) noexcept {
        if (this != &other) {
            // 先释放当前持有的资源
            if (acquired_ && sem_) {
                if (sem_post(sem_) != 0) {
                    LOG_ERROR << "sem_post failed during move assign: " << strerror(errno);
                }
            }
            sem_ = other.sem_;
            acquired_ = other.acquired_;
            other.sem_ = nullptr;
            other.acquired_ = false;
        }
        return *this;
    }

    bool acquired() const noexcept { return acquired_; }

    // 提前释放（可选），通常不需要手动调用
    void release() noexcept {
        if (acquired_) {
            if (sem_post(sem_) != 0) {
                LOG_ERROR << "sem_post failed in release: " << strerror(errno);
            }
            acquired_ = false;
        }
    }

private:
    sem_t* sem_;
    bool acquired_;
};

} // namespace DDS
} // namespace MB_DDF