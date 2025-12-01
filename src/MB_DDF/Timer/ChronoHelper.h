/**
 * @file ChronoHelper.h
 * @brief 高精度计时与区间统计工具。
 * @date 2025-10-19
 * @author Jiangkai
 *
 * 提供函数执行时间测量、分段计时、以及循环周期偏差统计（支持多计数器）。
 * 设计为跨平台实现，基于 `std::chrono::steady_clock`，避免系统时间调整影响。
 *
 * 使用示例：
 * - 单次计时：ChronoHelper::timing(func, args...)
 * - 平均计时：ChronoHelper::timingAverage(100, func, args...)
 * - 分段计时：ChronoHelper::clockStart(0); ...; ChronoHelper::clockEnd(0);
 * - 周期统计：ChronoHelper::record(0, 1000);
 */

 #pragma once

#include <chrono>
#include <unordered_map>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <vector>
#include <deque>
#include <iostream>
#include <iomanip>
#include <utility>

namespace MB_DDF {
namespace Timer {

/**
 * @class ChronoHelper
 * @brief 提供计时和周期统计接口，禁止嵌套计时。
 *
 * 线程安全说明：
 * - 单次/平均计时通过 thread_local `call_depth` 防止嵌套。
 * - 周期统计的全局状态（统计表、期望周期表）未加锁，默认用于单线程或外部同步场景。
 */
class ChronoHelper {
public:

    // 测量单次函数执行时间（禁止嵌套）
    /**
     * @brief 计量一次函数执行耗时并打印（单位：us）。
     * @tparam Func 可调用对象类型
     * @tparam Args 参数类型
     * @param func 可调用对象
     * @param args 参数列表
     */
    template<typename Func, typename... Args>
    static void timing(Func&& func, Args&&... args) {
        check_nested_call();

        ScopeGuard guard;
        auto duration_ns = measure_execution(std::forward<Func>(func),
                                                   std::forward<Args>(args)...);
        auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(duration_ns);

        std::cout << "Function executed in " << duration_us.count() << " us\n";
    }

    // 测量多次平均耗时并返回（ms）
    /**
     * @brief 多次执行求平均耗时（返回毫秒）。
     * @param times 执行次数（>0）
     * @return 平均耗时（ms）
     */
    template<typename Func, typename... Args>
    static double timingAverageReturn(int times, Func&& func, Args&&... args) {
        if (times <= 0) {
            std::cout << "[ChronoHelper Error] Invalid times parameter: " << times << "\n";
            std::abort();
        }

        check_nested_call();
        ScopeGuard guard;

        auto total = std::chrono::nanoseconds::zero();
        for (int i = 0; i < times; ++i) {
            auto duration = measure_execution(std::forward<Func>(func),
                                                    std::forward<Args>(args)...);
            total += duration;
        }

        return (double)total.count() / times / 1000000.0;
    }

    // 测量多次平均耗时并打印
    /**
     * @brief 多次执行求平均耗时并按量级自动选择单位（us/ms/s）打印。
     * @param times 执行次数（>0）
     */
    template<typename Func, typename... Args>
    static void timingAverage(int times, Func&& func, Args&&... args) {
        double time_ms = timingAverageReturn(times, std::forward<Func>(func), std::forward<Args>(args)...);
        if (time_ms < 1.0) {
            std::cout << "Average time (in " << times << " runs): " << std::fixed << std::setprecision(2)
                      << (time_ms * 1000.0) << " us\n";
        } else if (time_ms < 1000.0) {
            std::cout << "Average time (in " << times << " runs): " << std::fixed << std::setprecision(2)
                      << time_ms << " ms\n";
        } else {
            std::cout << "Average time (in " << times << " runs): " << std::fixed << std::setprecision(2)
                      << (time_ms / 1000.0) << " s\n";
        }
    }

    // 分段计时方法
    /**
     * @brief 记录分段计时起点。
     * @param n 分段ID
     */
    static void clockStart(unsigned n) {
        start_times[n] = Clock::now();
    }
    /**
     * @brief 记录分段计时终点并打印耗时（us）。
     * @param n 分段ID
     */
    static void clockEnd(unsigned n) {
        auto end = Clock::now();
        auto it = start_times.find(n);

        if (it == start_times.end()) {
            std::cout << "[ChronoHelper Error] Invalid clock ID: " << n << "\n";
            return; // 安全恢复执行
        }

        auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - it->second);

        std::cout << "[Clock " << n << "] Duration: " << duration_us.count() << " us\n";
        start_times.erase(it);
    }

    // ================== 区间统计功能（原IntervalStatistics） ==================
    /**
     * @brief 记录一次调用，用于统计循环周期与期望值的偏差。
     * @param counter_id 计数器ID（默认0，最多10）
     * @param expected_interval 期望周期（微秒，0表示按最近1秒平均值估算）
     * @return 是否达到记录周期
     */
    static bool record(int counter_id = 0, long long expected_interval = 0);
    /**
     * @brief 设置是否覆盖输出（终端上方回写）。
     * @param overwrite true开启覆盖输出
     */
    static void set_overwrite_output(bool overwrite);
    /**
     * @brief 重置指定计数器的统计。
     * @param counter_id 计数器ID
     */
    static void reset(int counter_id);
    /**
     * @brief 关闭/开启统计。
     * @param off true关闭；false开启
     */
    static void set_off(bool off);    

    // ========== 区间统计相关成员 ==========
    using Clock = std::chrono::steady_clock;
    struct Stats {
        std::deque<std::pair<Clock::time_point, long long>> recent_intervals; // 用于计算平均间隔
        Clock::time_point last_call_time;
        std::vector<long long> jitters;        // 存储抖动绝对值
        long long max_jitter = 0;              // 最大抖动
    };

    static std::unordered_map<int, Stats> stats_map;

private:
    // 嵌套调用防护
    static thread_local int call_depth; // 支持多线程场景

    static void check_nested_call() {
        if (call_depth > 0) {
            std::cout << "[ChronoHelper Error] Nested timing calls detected\n";
            std::abort();
        }
        ++call_depth;
    }

    // RAII 作用域守卫
    struct ScopeGuard {
        ~ScopeGuard() { --call_depth; }
    };

    // 时间测量核心逻辑
    template<typename Func, typename... Args>
    static auto measure_execution(Func&& func, Args&&... args)
        -> std::chrono::nanoseconds
    {
        auto start = Clock::now();
        std::forward<Func>(func)(std::forward<Args>(args)...);
        auto end = Clock::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    }

    static std::unordered_map<unsigned, Clock::time_point> start_times;
    static std::unordered_map<int, long long> expected_interval_map;
    static long long REPORT_INTERVAL;
    static Clock::time_point common_last_report_time;
    static bool off;
    static bool overwrite_output;
    static std::string last_output;

    static long long time_diff_us(const Clock::time_point& start, const Clock::time_point& end);
    static void report_all(const Clock::time_point& now);
    static long long calculate_average_interval(Stats& s);
    static void clean_old_intervals(Stats& s, const Clock::time_point& now);
    static long long calculate_percentile(const std::vector<long long>& sorted_jitters, double percentile);
};

}   // namespace Timer
}   // namespace MB_DDF
