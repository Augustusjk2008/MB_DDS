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

namespace MB_DDF {
namespace Timer {

namespace {
std::unordered_set<int> g_installed_signals;
std::mutex g_install_mtx;
}

SystemTimer::SystemTimer(std::function<void()> cb, const SystemTimerOptions& opt)
    : signal_no_(opt.signal_no),
      use_new_thread_(opt.use_new_thread),
      callback_(std::move(cb)) {}

SystemTimer::~SystemTimer() {
    stop();
}

std::unique_ptr<SystemTimer> SystemTimer::start(const std::string& period_str,
                                                std::function<void()> callback,
                                                const SystemTimerOptions& opt) {
    if (!callback) throw std::invalid_argument("callback must not be empty");

    auto timer = std::unique_ptr<SystemTimer>(new SystemTimer(std::move(callback), opt));

    // 安装信号处理器（按信号编号只安装一次）
    installHandler(opt.signal_no);

    // 如果使用独立线程，创建 eventfd + 线程，并应用调度与绑核
    if (opt.use_new_thread) {
        timer->event_fd_ = eventfd(0, EFD_CLOEXEC); // 阻塞读取以降低CPU占用
        if (timer->event_fd_ < 0) {
            throw std::runtime_error(std::string("eventfd failed: ") + std::strerror(errno));
        }
        timer->worker_.emplace([timer_ptr = timer.get(), policy = opt.sched_policy, prio = opt.priority, cpu = opt.cpu](){
            // 配置线程调度与绑核（在线程自身上下文）
            configureThread(pthread_self(), policy, prio, cpu);
            timer_ptr->workerLoop();
        });
        // 记录线程原生句柄以供 const 查询
        timer->worker_handle_ = timer->worker_->native_handle();
        timer->worker_handle_valid_ = true;
    }

    // 创建 POSIX 定时器
    sigevent sev{};
    memset(&sev, 0, sizeof(sev));
#ifdef SIGEV_THREAD_ID
    // 将信号发送到当前线程 TID（更精准），否则退化为 SIGEV_SIGNAL
    sev.sigev_notify = SIGEV_THREAD_ID;
    sev.sigev_signo = opt.signal_no;
    sev._sigev_un._tid = gettid();
#else
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = opt.signal_no;
#endif
    sev.sigev_value.sival_ptr = timer.get();

    if (timer_create(CLOCK_MONOTONIC, &sev, &timer->timer_id_) != 0) {
        throw std::runtime_error(std::string("timer_create failed: ") + std::strerror(errno));
    }

    // 解析周期字符串
    long long period_ns = parsePeriodNs(period_str);
    if (period_ns <= 0) {
        throw std::invalid_argument("invalid period string: " + period_str);
    }
    timespec ts = nsToTimespec(period_ns);

    itimerspec its{};
    its.it_value = ts;     // 首次到期
    its.it_interval = ts;  // 周期

    if (timer_settime(timer->timer_id_, 0, &its, nullptr) != 0) {
        throw std::runtime_error(std::string("timer_settime failed: ") + std::strerror(errno));
    }

    timer->running_ = true;
    return timer;
}

void SystemTimer::stop() {
    if (!running_) return;
    running_ = false;
    // 停止定时器
    itimerspec its{};
    memset(&its, 0, sizeof(its));
    timer_settime(timer_id_, 0, &its, nullptr);
    timer_delete(timer_id_);
    // 关闭线程与事件
    if (worker_.has_value()) {
        if (event_fd_ >= 0) {
            uint64_t one = 1;
            ssize_t wr;
            do {
                wr = ::write(event_fd_, &one, sizeof(one)); // 唤醒退出
            } while (wr < 0 && errno == EINTR);
            (void)wr;
            ::close(event_fd_);
            event_fd_ = -1;
        }
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
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
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
    if (self->use_new_thread_) {
        // 推送到 eventfd，由工作线程消费
        if (self->event_fd_ >= 0) {
            uint64_t one = 1;
            ssize_t wr;
            do {
                wr = ::write(self->event_fd_, &one, sizeof(one));
            } while (wr < 0 && errno == EINTR);
            (void)wr;
        }
    } else {
        // 在信号处理上下文中直接调用（简易场景）
        self->invokeFromSignal();
    }
}

void SystemTimer::invokeFromSignal() {
    try {
        if (callback_) callback_();
    } catch (...) {
        // 信号上下文不宜抛出，忽略异常
    }
}

void SystemTimer::workerLoop() {
    // 在独立线程中阻塞读取 eventfd
    while (true) {
        uint64_t cnt = 0;
        ssize_t r = ::read(event_fd_, &cnt, sizeof(cnt));
        if (r <= 0) {
            if (errno == EINTR) {
                continue;
            }
            break; // 事件关闭或错误，退出线程
        }
        if (!running_) break;
        try {
            if (callback_) callback_();
        } catch (...) {
            // 保护回调异常
        }
    }
}

void SystemTimer::configureThread(pthread_t th, int policy, int priority, int cpu) {
    // 设置调度策略与优先级
    sched_param sp{};
    sp.sched_priority = priority;
    if (pthread_setschedparam(th, policy, &sp) != 0) {
        // 可能需要 root 权限；失败则忽略但告知（不抛异常）
        // std::cerr << "pthread_setschedparam failed: " << std::strerror(errno) << std::endl;
    }
    // 绑核
    if (cpu >= 0) {
        // 检查CPU编号是否超出系统最大核心数
        int num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
        if (cpu >= num_cpus) {
            // CPU编号超出范围，记录错误但不抛异常（保持原有行为）
            // std::cerr << "Invalid CPU ID: " << cpu << ", available CPUs: 0-" << (num_cpus - 1) << std::endl;
            return;
        }
        
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(cpu, &cpuset);
        if (pthread_setaffinity_np(th, sizeof(cpu_set_t), &cpuset) != 0) {
            // 绑核失败，记录错误但不抛异常
            // std::cerr << "pthread_setaffinity_np failed: " << std::strerror(errno) << std::endl;
        }
    }
}

} // namespace Timer
} // namespace MB_DDF