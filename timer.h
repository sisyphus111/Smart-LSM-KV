//
// Created by 31049 on 2025/6/4.
//

#ifndef TIMER_H
#define TIMER_H

#include <chrono>
#include <unordered_map>
#include <string>
#include <iostream>
#include <iomanip>

class Timer {
private:
    struct TimerStats {
        int count = 0;
        double total_time = 0.0; // milliseconds
    };

    static std::unordered_map<std::string, TimerStats> stats;
    static std::unordered_map<std::string, std::chrono::high_resolution_clock::time_point> start_times;

public:
    // 开始计时
    static void start(const std::string& label) {
        start_times[label] = std::chrono::high_resolution_clock::now();
    }

    // 结束计时并记录
    static void end(const std::string& label) {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto it = start_times.find(label);
        if (it != start_times.end()) {
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - it->second);
            double time_ms = duration.count() / 1000.0; // 转换为毫秒
            
            auto& stat = stats[label];
            stat.count++;
            stat.total_time += time_ms;
            
            start_times.erase(it); // 清理已使用的开始时间
        }
    }

    // 打印所有统计信息
    static void print_stats() {
        if (stats.empty()) {
            std::cout << "No timer statistics available." << std::endl;
            return;
        }

        std::cout << "\n=== Timer Statistics ===" << std::endl;
        std::cout << std::left << std::setw(30) << "Function/Module"
                  << std::setw(10) << "Count"
                  << std::setw(15) << "Total(ms)"
                  << std::setw(15) << "Average(ms)" << std::endl;
        std::cout << std::string(70, '-') << std::endl;

        for (const auto& [label, stat] : stats) {
            double avg_time = stat.total_time / stat.count;
            std::cout << std::left << std::setw(30) << label
                      << std::setw(10) << stat.count
                      << std::setw(15) << std::fixed << std::setprecision(3) << stat.total_time
                      << std::setw(15) << std::fixed << std::setprecision(3) << avg_time << std::endl;
        }
        std::cout << std::string(70, '=') << std::endl;
    }

    // 清空统计信息
    static void clear_stats() {
        stats.clear();
        start_times.clear();
    }
};

// 静态成员定义
std::unordered_map<std::string, Timer::TimerStats> Timer::stats;
std::unordered_map<std::string, std::chrono::high_resolution_clock::time_point> Timer::start_times;

// 在程序退出时自动打印统计信息的类
class TimerReporter {
public:
    ~TimerReporter() {
        Timer::print_stats();
    }
};

// 全局对象，确保程序退出时打印统计信息
static TimerReporter global_timer_reporter;

#endif // TIMER_H
