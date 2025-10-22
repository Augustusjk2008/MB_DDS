/**
 * @file SystemTimer.cpp
 * @brief 高精度系统定时器类实现
 * @date 2025-10-19
 * @author Jiangkai
 */

#include "MB_DDF/Timer/SystemTimer.h"
#include <stdexcept>
#include <cstring>
#include <cerrno>
#include <unordered_set>
#include <mutex>
#include <cctype>

namespace MB_DDF {
namespace Timer {

namespace {
std::unordered_set<int> g_installed_signals;
std::mutex g_install_mtx;
}

SystemTimer::SystemTimer(std::function<void()> cb, const SystemTimerOptions& opt)
    : signal_no_(opt.signal_no),
      callback_(std::move(cb)) {}

SystemTimer::~SystemTimer() {
    stop();
}

std::unique_ptr<SystemTimer> SystemTimer::start(const std::string& period_str,
                                                std::function<void()> callback,
                                                const SystemTimerOptions& opt) {
    if (!callback) throw std::invalid_argument("callback must not be empty");

    auto timer = std::unique_ptr<SystemTimer>(new SystemTimer(std::move(callback), opt));

    // 解析周期字符串
    timer->period_ns_ = parsePeriodNs(period_str);
    if (timer->period_ns_ <= 0) {
        throw std::invalid_argument("invalid period string: " + period_str);
    }

    // 在当前线程先阻塞该实时信号，确保后续只由定时线程接收
    {
        sigset_t sigset;
        sigemptyset(&sigset);
        sigaddset(&sigset, timer->signal_no_);
        pthread_sigmask(SIG_BLOCK, &sigset, nullptr);
    }

    // 启动定时线程：在线程内安装信号处理器、解阻塞信号、创建定时器
    timer->worker_.emplace([timer_ptr = timer.get(), policy = opt.sched_policy, prio = opt.priority, cpu = opt.cpu]() {
        // 设置线程调度与绑核
        configureThread(pthread_self(), policy, prio, cpu);

        // 定时线程解阻塞该信号
        sigset_t sigset;
        sigemptyset(&sigset);
        sigaddset(&sigset, timer_ptr->signal_no_);
        pthread_sigmask(SIG_UNBLOCK, &sigset, nullptr);

        // 安装信号处理器（按信号编号只安装一次）
        installHandler(timer_ptr->signal_no_);

        // 创建 POSIX 定时器：信号通知到当前线程（SIGEV_SIGNAL）
        sigevent sev{};
        memset(&sev, 0, sizeof(sev));
        sev.sigev_notify = SIGEV_SIGNAL;
        sev.sigev_signo = timer_ptr->signal_no_;
        sev.sigev_value.sival_ptr = timer_ptr;

        if (timer_create(CLOCK_MONOTONIC, &sev, &timer_ptr->timer_id_) != 0) {
            // 无法创建定时器，线程直接返回
            return;
        }

        // 设置定时参数
        timespec ts = nsToTimespec(timer_ptr->period_ns_);
        itimerspec its{};
        its.it_value = ts;     // 首次到期
        its.it_interval = ts;  // 周期
        if (timer_settime(timer_ptr->timer_id_, 0, &its, nullptr) != 0) {
            // 设置失败，删除定时器并返回
            timer_delete(timer_ptr->timer_id_);
            return;
        }

        timer_ptr->running_ = true;

        // 线程保持存活，直到 stop() 取消定时器并置 running_ 为 false
        while (true) {
            if (!timer_ptr->running_) break;
            ::sleep(1);
        }
    });

    // 缓存线程原生句柄以供 const 查询
    timer->worker_handle_ = timer->worker_->native_handle();
    timer->worker_handle_valid_ = true;

    return timer;
}

void SystemTimer::stop() {
    if (!running_) {
        // 即便未运行，也要安全地回收线程资源
        if (worker_.has_value()) {
            // 如果线程仍在等待，置运行标志为 false 以促退出
            running_ = false;
            if (worker_->joinable()) worker_->join();
            worker_.reset();
            worker_handle_valid_ = false;
        }
        return;
    }

    running_ = false;

    // 停止并删除定时器
    itimerspec its{};
    memset(&its, 0, sizeof(its));
    timer_settime(timer_id_, 0, &its, nullptr);
    timer_delete(timer_id_);

    // 等待线程结束
    if (worker_.has_value()) {
        if (worker_->joinable()) worker_->join();
        worker_.reset();
        worker_handle_valid_ = false;
    }
}

std::optional<pthread_t> SystemTimer::workerHandle() const {
    if (!worker_handle_valid_) return std::nullopt;
    return worker_handle_;
}

long long SystemTimer::parsePeriodNs(const std::string& period) {
    // 允许 "100ms", "1s", "500us", "100000ns" 等
    std::string s;
    s.reserve(period.size());
    for (char c : period) {
        if (!std::isspace(static_cast<unsigned char>(c))) s.push_back(c);
    }
    if (s.empty()) return -1;

    // 找到后缀位置
    size_t pos = s.find_first_not_of("0123456789");
    if (pos == std::string::npos || pos == 0) return -1;

    long long value = 0;
    try {
        value = std::stoll(s.substr(0, pos));
    } catch (...) {
        return -1;
    }
    std::string unit = s.substr(pos);
    if (unit == "s") {
        return value * 1000000000LL;
    } else if (unit == "ms") {
        return value * 1000000LL;
    } else if (unit == "us") {
        return value * 1000LL;
    } else if (unit == "ns") {
        return value;
    }
    return -1;
}

timespec SystemTimer::nsToTimespec(long long ns) {
    timespec ts{};
    ts.tv_sec = ns / 1000000000LL;
    ts.tv_nsec = ns % 1000000000LL;
    return ts;
}

void SystemTimer::installHandler(int signo) {
    std::lock_guard<std::mutex> lk(g_install_mtx);
    if (g_installed_signals.find(signo) != g_installed_signals.end()) return;
    struct sigaction sa{};
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = &SystemTimer::signalHandler;
    sa.sa_flags = SA_SIGINFO; // 参考实现不使用 SA_RESTART
    sigemptyset(&sa.sa_mask);
    if (sigaction(signo, &sa, nullptr) != 0) {
        throw std::runtime_error(std::string("sigaction failed: ") + std::strerror(errno));
    }
    g_installed_signals.insert(signo);
}

void SystemTimer::signalHandler(int /*signo*/, siginfo_t* info, void* /*ucontext*/) {
    if (!info) return;
    auto* self = reinterpret_cast<SystemTimer*>(info->si_value.sival_ptr);
    if (!self) return;
    self->invokeFromSignal();
}

void SystemTimer::invokeFromSignal() {
    if (callback_) {
        callback_();
    }
}

void SystemTimer::configureThread(pthread_t th, int policy, int priority, int cpu) {
    // 设置调度策略与优先级
    sched_param sp{};
    sp.sched_priority = priority;
    if (pthread_setschedparam(th, policy, &sp) != 0) {
        // 可能需要 root 权限；失败则打印提示但不抛异常
        // 可按需改为日志：std::cerr << "pthread_setschedparam failed: " << std::strerror(errno) << std::endl;
    }
    // 绑核
    if (cpu >= 0) {
        int num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
        if (cpu >= num_cpus) {
            return;
        }
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(cpu, &cpuset);
        pthread_setaffinity_np(th, sizeof(cpu_set_t), &cpuset);
    }
}

} // namespace Timer
} // namespace MB_DDF