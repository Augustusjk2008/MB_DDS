/**
 * @file SystemTimer.h
 * @brief 高精度系统定时器类
 * @date 2025-10-19
 * @author Jiangkai
 */

#pragma once
#include <signal.h>
#include <time.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#include <functional>
#include <string>
#include <thread>
#include <optional>
#include <memory>

namespace MB_DDF {
namespace Timer {

struct SystemTimerOptions {
    int sched_policy = SCHED_FIFO;        // 定时线程调度策略
    int priority = 50;                    // 定时线程优先级（SCHED_FIFO/RR 范围内有效）
    int cpu = -1;                         // 绑核编号，-1 表示不绑核
    int signal_no = SIGRTMIN;             // 使用的实时信号编号
};

class SystemTimer {
public:
    // 一次性接口：解析周期字符串、安装信号、创建并启动高精度周期定时器
    static std::unique_ptr<SystemTimer> start(const std::string& period_str,
                                              std::function<void()> callback,
                                              const SystemTimerOptions& opt = {});

    ~SystemTimer();

    // 停止并删除定时器
    void stop();

    // 定时器重新计时
    void reset();

    // 是否正在运行
    bool isRunning() const { return running_; }

    // 获取定时线程的线程句柄（如使用）
    std::optional<pthread_t> workerHandle() const;

    // 设定线程调度与绑核
    static void configureThread(pthread_t th, int policy, int priority, int cpu);

private:
    explicit SystemTimer(std::function<void()> cb, const SystemTimerOptions& opt);

    // 解析周期字符串（支持 s/ms/us/ns），返回纳秒
    static long long parsePeriodNs(const std::string& period);
    static timespec nsToTimespec(long long ns);

    // 安装信号处理（仅安装一次）
    static void installHandler(int signo);
    static void signalHandler(int signo, siginfo_t* info, void* ucontext);

    // 在信号处理上下文中触发回调
    void invokeFromSignal();

private:
    timer_t timer_id_{};
    bool running_ = false;
    int signal_no_ = SIGRTMIN;

    std::function<void()> callback_;

    std::optional<std::thread> worker_;
    pthread_t worker_handle_{};         // 缓存原生句柄以支持 const 访问
    bool worker_handle_valid_ = false;

    long long period_ns_ = 0;           // 解析后的周期（纳秒）
};

} // namespace Timer
} // namespace MB_DDF