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

// 处理所有统计器的报告
void ChronoHelper::report_all(const Clock::time_point& now) {
    common_last_report_time = now;
    std::ostringstream oss;
    for (auto& pair : stats_map) {
        int counter_id = pair.first;
        Stats& s = pair.second;
        if (s.count == 0) continue;
        double avg_dev = (double)s.total_dev / (double)s.count;
        long long expected_interval = expected_interval_map[counter_id];
        if (expected_interval == 0 && !s.recent_intervals.empty()) {
            expected_interval = calculate_average_interval(s);
        }
        oss << "Timer #" << counter_id
            << " | Set: " << std::left << std::setw(8) << expected_interval
            << " us | MaxErr: " << std::setw(8) << s.max_positive
            << " us | MinErr: " << std::setw(8) << s.max_negative
            << " us | AvgErr: " << std::fixed << std::setprecision(2) << avg_dev << " us\n";
        s.max_positive = 0;
        s.max_negative = 0;
        s.total_dev = 0;
        s.count = 0;
    }
    std::string output = oss.str();
    if (output.empty()) return;
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
void ChronoHelper::record(int counter_id, long long expected_interval) {
    if (off || counter_id < 0 || counter_id > 10) return;
    auto now = Clock::now();
    if (expected_interval > 0) {
        expected_interval_map[counter_id] = expected_interval;
    }
    if (stats_map.find(counter_id) == stats_map.end()) {
        stats_map[counter_id] = Stats();
        stats_map[counter_id].last_call_time = now;
        return;
    }
    Stats& s = stats_map[counter_id];
    long long actual_interval = time_diff_us(s.last_call_time, now);
    if (expected_interval == 0) {
        s.recent_intervals.emplace_back(now, actual_interval);
        clean_old_intervals(s, now);
        expected_interval = calculate_average_interval(s);
        expected_interval_map[counter_id] = expected_interval;
    }
    long long deviation = actual_interval - expected_interval;
    if (deviation > s.max_positive) s.max_positive = deviation;
    if (deviation < s.max_negative) s.max_negative = deviation;
    s.total_dev += deviation;
    s.count++;
    s.last_call_time = now;
    if (time_diff_us(common_last_report_time, now) >= REPORT_INTERVAL) {
        report_all(now);
    }
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