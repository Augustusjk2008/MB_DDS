/**
 * @file ChronoHelper.cpp
 * @brief 高精度计时与区间统计工具实现
 * @date 2025-10-19
 * @author Jiangkai
 */

#include "MB_DDF/Timer/ChronoHelper.h"
#include <iomanip>
#include <iostream>
#include <algorithm>
#include <sstream>
#include <vector>
#include <cmath>

namespace MB_DDF {
namespace Timer {
    
// 静态成员初始化
thread_local int ChronoHelper::call_depth = 0;
std::unordered_map<unsigned, ChronoHelper::Clock::time_point> ChronoHelper::start_times;

// 区间统计相关静态成员初始化
std::unordered_map<int, ChronoHelper::Stats> ChronoHelper::stats_map;
std::unordered_map<int, long long> ChronoHelper::expected_interval_map;
long long ChronoHelper::REPORT_INTERVAL = 1000000; // 1秒（1000000微秒）
ChronoHelper::Clock::time_point ChronoHelper::common_last_report_time = ChronoHelper::Clock::now();
bool ChronoHelper::off = false;
bool ChronoHelper::overwrite_output = false;
std::string ChronoHelper::last_output;

// 计算时间差（微秒）
long long ChronoHelper::time_diff_us(const Clock::time_point& start, const Clock::time_point& end) {
    return std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
}

// 计算1秒内的平均循环时间
long long ChronoHelper::calculate_average_interval(Stats& s) {
    if (s.recent_intervals.empty()) return 0;
    long long total_interval = 0;
    for (const auto& entry : s.recent_intervals) {
        total_interval += entry.second;
    }
    if (total_interval % s.recent_intervals.size() != 0) {
        return total_interval / s.recent_intervals.size() + 1;
    } else {
        return total_interval / s.recent_intervals.size();
    }
}

// 清理超过1秒的历史数据
void ChronoHelper::clean_old_intervals(Stats& s, const Clock::time_point& now) {
    if (s.recent_intervals.empty()) return;
    while (!s.recent_intervals.empty()) {
        const auto& oldest = s.recent_intervals.front();
        if (time_diff_us(oldest.first, now) > 1000000) {
            s.recent_intervals.pop_front();
        } else {
            break;
        }
    }
}

// 计算百分位数
long long ChronoHelper::calculate_percentile(const std::vector<long long>& sorted_jitters, double percentile) {
    if (sorted_jitters.empty()) return 0;
    
    if (percentile <= 0.0) return sorted_jitters.front();
    if (percentile >= 1.0) return sorted_jitters.back();
    
    // 使用线性插值方法计算百分位数
    double position = percentile * (sorted_jitters.size() - 1);
    size_t lower_index = static_cast<size_t>(std::floor(position));
    size_t upper_index = static_cast<size_t>(std::ceil(position));
    
    if (lower_index == upper_index) {
        return sorted_jitters[lower_index];
    }
    
    // 线性插值
    double fraction = position - lower_index;
    return static_cast<long long>((1.0 - fraction) * sorted_jitters[lower_index] + 
                                  fraction * sorted_jitters[upper_index]);
}

// 处理所有统计器的报告
void ChronoHelper::report_all(const Clock::time_point& now) {
    // 记录报告开始时间，用于剔除计算耗时
    auto report_start_time = Clock::now();
    
    common_last_report_time = now;
    std::ostringstream oss;
    
    for (auto& pair : stats_map) {
        int counter_id = pair.first;
        Stats& s = pair.second;
        
        if (s.jitters.empty()) continue;
        
        // 复制当前抖动数据并排序，用于计算百分位数
        std::vector<long long> sorted_jitters = s.jitters;
        std::sort(sorted_jitters.begin(), sorted_jitters.end());
        
        long long expected_interval = expected_interval_map[counter_id];
        if (expected_interval == 0 && !s.recent_intervals.empty()) {
            expected_interval = calculate_average_interval(s);
        }
        
        // 计算统计指标
        long long max_jitter = sorted_jitters.back(); // 最大值就是最后一个元素
        long long p99_jitter = calculate_percentile(sorted_jitters, 0.999);
        long long p95_jitter = calculate_percentile(sorted_jitters, 0.95);
        long long p70_jitter = calculate_percentile(sorted_jitters, 0.70);
        
        oss << "Timer #" << counter_id
            << " | Set: " << std::left << std::setw(6) << expected_interval
            << " us | Max: " << std::setw(6) << max_jitter
            << " us | P99.9: " << std::setw(6) << p99_jitter
            << " us | P95: " << std::setw(6) << p95_jitter
            << " us | P70: " << std::setw(6) << p70_jitter << " us\n";
        
        // 清空当前统计周期的数据，为下一个周期做准备
        s.jitters.clear();
        s.max_jitter = 0;
    }
    
    std::string output = oss.str();
    if (output.empty()) return;
    
    // 计算并剔除报告本身的耗时
    auto report_end_time = Clock::now();
    long long report_duration = time_diff_us(report_start_time, report_end_time);
    
    // 更新最后报告时间，剔除报告耗时的影响
    common_last_report_time = common_last_report_time + std::chrono::microseconds(report_duration);
    
    if (overwrite_output && !last_output.empty()) {
        int line_count = std::count(last_output.begin(), last_output.end(), '\n');
        std::cout << "\033[" << line_count << "A";
    }
    std::cout << output;
    if (overwrite_output) {
        std::cout << std::flush;
        last_output = output;
    } else {
        last_output.clear();
    }
}

// 记录时间间隔点
bool ChronoHelper::record(int counter_id, long long expected_interval) {
    if (off || counter_id < 0 || counter_id > 10) return false;
    auto now = Clock::now();
    
    if (expected_interval > 0) {
        expected_interval_map[counter_id] = expected_interval;
    }
    
    if (stats_map.find(counter_id) == stats_map.end()) {
        stats_map[counter_id] = Stats();
        stats_map[counter_id].last_call_time = now;
        return false;
    }
    
    Stats& s = stats_map[counter_id];
    long long actual_interval = time_diff_us(s.last_call_time, now);
    
    if (expected_interval == 0) {
        s.recent_intervals.emplace_back(now, actual_interval);
        clean_old_intervals(s, now);
        expected_interval = calculate_average_interval(s);
        expected_interval_map[counter_id] = expected_interval;
    }
    
    // 计算抖动绝对值
    long long jitter = std::abs(actual_interval - expected_interval);
    
    // 更新统计
    if (jitter > s.max_jitter) s.max_jitter = jitter;
    s.jitters.push_back(jitter);
    s.last_call_time = now;
    
    if (time_diff_us(common_last_report_time, now) >= REPORT_INTERVAL) {
        report_all(now);
        return true;
    }
    return false;
}

void ChronoHelper::set_overwrite_output(bool overwrite) {
    overwrite_output = overwrite;
    if (!overwrite) last_output.clear();
}

void ChronoHelper::reset(int counter_id) {
    stats_map.erase(counter_id);
    expected_interval_map.erase(counter_id);
}

void ChronoHelper::set_off(bool v) {
    off = v;
}

}   // namespace Timer
}   // namespace MB_DDF