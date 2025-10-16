/**
 * @file LoggerExtensions.h
 * @brief 日志系统扩展功能
 * @version 1.0
 * @date 2024
 * 
 * @copyright Copyright (c) 2024 MB_DDF
 * 
 * @details
 * 本文件提供日志系统的扩展功能，包括各种常用的格式化打印功能。
 * 这些功能与原始的Logger.h完全解耦，即使不包含此文件，程序也能正常编译运行。
 * 
 * 主要功能包括：
 * - 打印空行
 * - 打印分隔符
 * - 打印标题横幅
 * - 打印进度提示
 * - 打印框架文本
 * - 打印时间戳标记
 * 
 * @par 使用方法:
 * @code
 * #include "LoggerExtensions.h"  // 可选包含
 * 
 * LOG_BLANK_LINE();              // 打印空行
 * LOG_SEPARATOR();               // 打印分隔符
 * LOG_TITLE("系统启动");          // 打印标题
 * LOG_PROGRESS("初始化", 50);     // 打印进度
 * @endcode
 */

#pragma once

#include "MB_DDF/Debug/Logger.h"
#include <string>
#include <sstream>
#include <iomanip>
#include <chrono>

namespace MB_DDF {
namespace Debug {
namespace Extensions {

/**
 * @brief 日志扩展工具类
 * 
 * 提供各种格式化打印功能的静态方法。
 * 所有方法都是线程安全的，因为它们最终都通过Logger单例进行输出。
 */
class LoggerExtensions {
public:
    /**
     * @brief 打印空行
     * @param level 日志级别，默认为INFO
     * @param count 空行数量，默认为1
     */
    static void print_blank_lines(LogLevel level = LogLevel::INFO, int count = 1) {
        for (int i = 0; i < count; ++i) {
            Logger::instance().log(level, "", __FILE__, __LINE__, __func__);
        }
    }

    /**
     * @brief 打印分隔符
     * @param level 日志级别，默认为INFO
     * @param char_symbol 分隔符字符，默认为'-'
     * @param length 分隔符长度，默认为80
     */
    static void print_separator(LogLevel level = LogLevel::INFO, char char_symbol = '-', int length = 80) {
        std::string separator(length, char_symbol);
        Logger::instance().log(level, separator, __FILE__, __LINE__, __func__);
    }

    /**
     * @brief 打印标题横幅
     * @param title 标题文本
     * @param level 日志级别，默认为INFO
     * @param char_symbol 边框字符，默认为'='
     * @param width 总宽度，默认为80
     */
    static void print_title(const std::string& title, LogLevel level = LogLevel::INFO, 
                           char char_symbol = '=', int width = 80) {
        std::string separator(width, char_symbol);
        Logger::instance().log(level, separator, __FILE__, __LINE__, __func__);
        
        // 计算居中位置
        int title_len = static_cast<int>(title.length());
        int padding = (width - title_len - 2) / 2;  // 减2是为了左右各留一个字符
        if (padding < 0) padding = 0;
        
        std::ostringstream oss;
        oss << char_symbol << std::string(padding, ' ') << title 
            << std::string(width - padding - title_len - 2, ' ') << char_symbol;
        Logger::instance().log(level, oss.str(), __FILE__, __LINE__, __func__);
        Logger::instance().log(level, separator, __FILE__, __LINE__, __func__);
    }

    /**
     * @brief 打印进度提示
     * @param task_name 任务名称
     * @param progress 进度百分比 (0-100)
     * @param level 日志级别，默认为INFO
     * @param width 进度条宽度，默认为50
     */
    static void print_progress(const std::string& task_name, int progress, 
                              LogLevel level = LogLevel::INFO, int width = 50) {
        if (progress < 0) progress = 0;
        if (progress > 100) progress = 100;
        
        int filled = (progress * width) / 100;
        int empty = width - filled;
        
        std::ostringstream oss;// 使用Unicode字符绘制进度条
        oss << task_name << " [" << std::string(filled, '#') << std::string(empty, '-') 
            << "] " << std::setw(3) << progress << "%";
        Logger::instance().log(level, oss.str(), __FILE__, __LINE__, __func__);
    }

    /**
     * @brief 打印框架文本
     * @param text 要框起来的文本
     * @param level 日志级别，默认为INFO
     * @param char_symbol 边框字符，默认为'*'
     */
    static void print_boxed_text(const std::string& text, LogLevel level = LogLevel::INFO, 
                                char char_symbol = '*') {
        int text_len = static_cast<int>(text.length());
        int box_width = text_len + 4;  // 左右各留2个字符的空间
        
        std::string top_bottom(box_width, char_symbol);
        std::ostringstream middle;
        middle << char_symbol << " " << text << " " << char_symbol;
        
        Logger::instance().log(level, top_bottom, __FILE__, __LINE__, __func__);
        Logger::instance().log(level, middle.str(), __FILE__, __LINE__, __func__);
        Logger::instance().log(level, top_bottom, __FILE__, __LINE__, __func__);
    }

    /**
     * @brief 打印时间戳标记
     * @param message 附加消息，默认为空
     * @param level 日志级别，默认为INFO
     */
    static void print_timestamp_marker(const std::string& message = "", LogLevel level = LogLevel::INFO) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        std::ostringstream oss;
        oss << "⏰ TIMESTAMP: " << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        oss << "." << std::setfill('0') << std::setw(3) << ms.count();
        
        if (!message.empty()) {
            oss << " - " << message;
        }
        
        Logger::instance().log(level, oss.str(), __FILE__, __LINE__, __func__);
    }

    /**
     * @brief 打印双线分隔符
     * @param level 日志级别，默认为INFO
     * @param length 分隔符长度，默认为80
     */
    static void print_double_separator(LogLevel level = LogLevel::INFO, int length = 80) {
        std::string separator(length, '=');
        Logger::instance().log(level, separator, __FILE__, __LINE__, __func__);
    }

    /**
     * @brief 打印点线分隔符
     * @param level 日志级别，默认为INFO
     * @param length 分隔符长度，默认为80
     */
    static void print_dotted_separator(LogLevel level = LogLevel::INFO, int length = 80) {
        std::string separator(length, '.');
        Logger::instance().log(level, separator, __FILE__, __LINE__, __func__);
    }
};

} // namespace Extensions
} // namespace Debug
} // namespace MB_DDF

// ================================ 便捷宏定义 ================================

/**
 * @defgroup LoggerExtensionMacros 日志扩展宏
 * @brief 提供便捷的日志格式化宏定义
 * @{
 */

/**
 * @def LOG_BLANK_LINE
 * @brief 打印空行宏
 */
#define LOG_BLANK_LINE() \
    MB_DDF::Debug::Extensions::LoggerExtensions::print_blank_lines(MB_DDF::Debug::LogLevel::INFO, 1)

/**
 * @def LOG_BLANK_LINES
 * @brief 打印多个空行宏
 * @param count 空行数量
 */
#define LOG_BLANK_LINES(count) \
    MB_DDF::Debug::Extensions::LoggerExtensions::print_blank_lines(MB_DDF::Debug::LogLevel::INFO, count)

/**
 * @def LOG_BLANK_LINE_DEBUG
 * @brief 打印DEBUG级别空行宏
 */
#define LOG_BLANK_LINE_DEBUG() \
    MB_DDF::Debug::Extensions::LoggerExtensions::print_blank_lines(MB_DDF::Debug::LogLevel::DEBUG, 1)

/**
 * @def LOG_SEPARATOR
 * @brief 打印分隔符宏
 */
#define LOG_SEPARATOR() \
    MB_DDF::Debug::Extensions::LoggerExtensions::print_separator(MB_DDF::Debug::LogLevel::INFO, '-', 80)

/**
 * @def LOG_SEPARATOR_CUSTOM
 * @brief 打印自定义分隔符宏
 * @param char_symbol 分隔符字符
 * @param length 长度
 */
#define LOG_SEPARATOR_CUSTOM(char_symbol, length) \
    MB_DDF::Debug::Extensions::LoggerExtensions::print_separator(MB_DDF::Debug::LogLevel::INFO, char_symbol, length)

/**
 * @def LOG_DOUBLE_SEPARATOR
 * @brief 打印双线分隔符宏
 */
#define LOG_DOUBLE_SEPARATOR() \
    MB_DDF::Debug::Extensions::LoggerExtensions::print_double_separator(MB_DDF::Debug::LogLevel::INFO, 80)

/**
 * @def LOG_DOUBLE_SEPARATOR_CUSTOM
 * @brief 打印自定义长度双线分隔符宏
 * @param length 长度
 */
#define LOG_DOUBLE_SEPARATOR_CUSTOM(length) \
    MB_DDF::Debug::Extensions::LoggerExtensions::print_double_separator(MB_DDF::Debug::LogLevel::INFO, length)

/**
 * @def LOG_DOTTED_SEPARATOR
 * @brief 打印点线分隔符宏
 */
#define LOG_DOTTED_SEPARATOR() \
    MB_DDF::Debug::Extensions::LoggerExtensions::print_dotted_separator(MB_DDF::Debug::LogLevel::INFO, 80)

/**
 * @def LOG_DOTTED_SEPARATOR_CUSTOM
 * @brief 打印自定义长度点线分隔符宏
 * @param length 长度
 */
#define LOG_DOTTED_SEPARATOR_CUSTOM(length) \
    MB_DDF::Debug::Extensions::LoggerExtensions::print_dotted_separator(MB_DDF::Debug::LogLevel::INFO, length)

/**
 * @def LOG_TITLE
 * @brief 打印标题宏
 * @param title 标题文本
 */
#define LOG_TITLE(title) \
    MB_DDF::Debug::Extensions::LoggerExtensions::print_title(title, MB_DDF::Debug::LogLevel::INFO, '=', 80)

/**
 * @def LOG_TITLE_CUSTOM
 * @brief 打印自定义标题宏
 * @param title 标题文本
 * @param char_symbol 边框字符
 * @param width 宽度
 */
#define LOG_TITLE_CUSTOM(title, char_symbol, width) \
    MB_DDF::Debug::Extensions::LoggerExtensions::print_title(title, MB_DDF::Debug::LogLevel::INFO, char_symbol, width)

/**
 * @def LOG_TITLE_WARN
 * @brief 打印警告级别标题宏
 * @param title 标题文本
 */
#define LOG_TITLE_WARN(title) \
    MB_DDF::Debug::Extensions::LoggerExtensions::print_title(title, MB_DDF::Debug::LogLevel::WARN, '=', 80)

/**
 * @def LOG_PROGRESS
 * @brief 打印进度宏
 * @param task_name 任务名称
 * @param progress 进度百分比
 */
#define LOG_PROGRESS(task_name, progress) \
    MB_DDF::Debug::Extensions::LoggerExtensions::print_progress(task_name, progress, MB_DDF::Debug::LogLevel::INFO, 50)

/**
 * @def LOG_PROGRESS_CUSTOM
 * @brief 打印自定义进度宏
 * @param task_name 任务名称
 * @param progress 进度百分比
 * @param width 进度条宽度
 */
#define LOG_PROGRESS_CUSTOM(task_name, progress, width) \
    MB_DDF::Debug::Extensions::LoggerExtensions::print_progress(task_name, progress, MB_DDF::Debug::LogLevel::INFO, width)

/**
 * @def LOG_BOX
 * @brief 打印框架文本宏
 * @param text 要框起来的文本
 */
#define LOG_BOX(text) \
    MB_DDF::Debug::Extensions::LoggerExtensions::print_boxed_text(text, MB_DDF::Debug::LogLevel::INFO, '*')

/**
 * @def LOG_BOX_CUSTOM
 * @brief 打印自定义框架文本宏
 * @param text 要框起来的文本
 * @param char_symbol 边框字符
 */
#define LOG_BOX_CUSTOM(text, char_symbol) \
    MB_DDF::Debug::Extensions::LoggerExtensions::print_boxed_text(text, MB_DDF::Debug::LogLevel::INFO, char_symbol)

/**
 * @def LOG_BOX_ERROR
 * @brief 打印错误级别框架文本宏
 * @param text 要框起来的文本
 */
#define LOG_BOX_ERROR(text) \
    MB_DDF::Debug::Extensions::LoggerExtensions::print_boxed_text(text, MB_DDF::Debug::LogLevel::ERROR, '*')

/**
 * @def LOG_TIMESTAMP
 * @brief 打印时间戳标记宏
 */
#define LOG_TIMESTAMP() \
    MB_DDF::Debug::Extensions::LoggerExtensions::print_timestamp_marker("", MB_DDF::Debug::LogLevel::INFO)

/**
 * @def LOG_TIMESTAMP_MSG
 * @brief 打印带消息的时间戳标记宏
 * @param message 附加消息
 */
#define LOG_TIMESTAMP_MSG(message) \
    MB_DDF::Debug::Extensions::LoggerExtensions::print_timestamp_marker(message, MB_DDF::Debug::LogLevel::INFO)

/** @} */ // end of LoggerExtensionMacros group
