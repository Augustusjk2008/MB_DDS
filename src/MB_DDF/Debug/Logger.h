/**
 * @file Logger.h
 * @brief 日志系统实现，提供多级别日志记录和输出功能
 * @date 2025-10-16
 * @author Jiangkai
 * 
 * 实现了一个线程安全的日志系统，支持多种日志级别、文件输出、
 * 自定义输出回调和流式日志记录。采用单例模式确保全局唯一性。
 */

#pragma once

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <ctime>
#include <unordered_map>
#include <functional>

namespace MB_DDF {
namespace Debug {

/**
 * @namespace ColorCodes
 * @brief ANSI颜色代码定义，用于控制台彩色输出
 */
namespace ColorCodes {
    constexpr const char* RESET   = "\033[0m";   ///< 重置颜色
    constexpr const char* GRAY    = "\033[90m";  ///< 灰色 - TRACE
    constexpr const char* CYAN    = "\033[36m";  ///< 青色 - DEBUG
    constexpr const char* GREEN   = "\033[32m";  ///< 绿色 - INFO
    constexpr const char* YELLOW  = "\033[33m";  ///< 黄色 - WARN
    constexpr const char* RED     = "\033[31m";  ///< 红色 - ERROR
    constexpr const char* MAGENTA = "\033[35m";  ///< 洋红色 - FATAL
    constexpr const char* BOLD    = "\033[1m";   ///< 粗体
}

/**
 * @enum LogLevel
 * @brief 日志级别枚举，定义了不同严重程度的日志类型
 * 
 * 日志级别从低到高排列，用于控制日志输出的详细程度。
 * 只有等于或高于设定级别的日志才会被输出。
 */
enum class LogLevel {
    TRACE,    ///< 跟踪级别，最详细的调试信息
    DEBUG,    ///< 调试级别，用于开发阶段的调试信息
    INFO,     ///< 信息级别，一般性的运行信息
    WARN,     ///< 警告级别，潜在的问题或异常情况
    ERROR,    ///< 错误级别，发生了错误但程序可以继续运行
    FATAL,    ///< 致命级别，严重错误导致程序无法继续运行
    OFF       ///< 关闭级别，不输出任何日志
};

/**
 * @class Logger
 * @brief 线程安全的日志记录器类，采用单例模式实现
 * 
 * 提供多级别日志记录功能，支持控制台输出、文件输出和自定义回调输出。
 * 使用单例模式确保全局唯一性，内部使用互斥锁保证线程安全。
 */
class Logger {
public:
    /**
     * @typedef OutputCallback
     * @brief 自定义输出回调函数类型定义
     * 
     * 回调函数接收日志级别和格式化后的日志消息作为参数
     */
    using OutputCallback = std::function<void(LogLevel, const std::string&)>;
    
    /**
     * @brief 获取Logger单例实例
     * @return Logger的唯一实例引用
     * 
     * 使用局部静态变量实现线程安全的单例模式，
     * 保证在多线程环境下只创建一个Logger实例。
     */
    static Logger& instance() {
        static Logger logger;
        return logger;
    }

    /**
     * @brief 设置全局日志级别
     * @param level 要设置的日志级别
     * 
     * 只有等于或高于设定级别的日志才会被输出。
     * 此操作是线程安全的。
     */
    void set_level(LogLevel level) {
        std::lock_guard<std::mutex> lock(mutex_);
        level_ = level;
    }

    /**
     * @brief 添加自定义输出回调函数
     * @param callback 回调函数，接收日志级别和格式化消息
     * 
     * 可以添加多个回调函数，每次日志输出时都会调用所有已注册的回调。
     * 此操作是线程安全的。
     */
    void add_callback(OutputCallback callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        callbacks_.push_back(std::move(callback));
    }

    /**
     * @brief 设置是否启用时间戳输出
     * @param enabled true启用时间戳，false禁用时间戳
     * 
     * 控制日志输出中是否包含时间戳信息。
     * 此操作是线程安全的。
     */
    void set_timestamp_enabled(bool enabled) {
        std::lock_guard<std::mutex> lock(mutex_);
        enable_timestamp_ = enabled;
    }

    /**
     * @brief 设置彩色输出开关
     * @param enabled true启用彩色输出，false禁用彩色输出
     * 
     * 控制日志输出是否使用ANSI颜色代码。此操作是线程安全的。
     */
    void set_color_enabled(bool enabled) {
        std::lock_guard<std::mutex> lock(mutex_);
        enable_color_ = enabled;
    }

    /**
     * @brief 设置函数名和行号显示开关
     * @param enabled true启用函数名和行号显示，false禁用
     * 
     * 控制日志输出是否包含[函数名:行号]信息。此操作是线程安全的。
     */
    void set_function_line_enabled(bool enabled) {
        std::lock_guard<std::mutex> lock(mutex_);
        enable_function_line_ = enabled;
    }

    /**
     * @brief 设置日志文件输出
     * @param filename 日志文件路径
     * 
     * 以追加模式打开指定文件用于日志输出。
     * 如果文件不存在会自动创建，此操作是线程安全的。
     */
    void set_file_output(const std::string& filename) {
        std::lock_guard<std::mutex> lock(mutex_);
        file_.open(filename, std::ios::out | std::ios::app);
    }

    /**
     * @brief 记录日志消息
     * @param level 日志级别
     * @param message 日志消息内容
     * @param file 源文件名
     * @param line 源文件行号
     * @param function 函数名
     * 
     * 根据日志级别决定是否输出日志。输出目标包括：
     * - 控制台（WARN及以上级别输出到stderr，其他输出到stdout）
     * - 文件（如果已设置文件输出）
     * - 自定义回调函数
     */
    void log(LogLevel level, const std::string& message, 
             const char* file, int line, const char* function) {
        if (level < level_ || level == LogLevel::OFF) return;

        std::string formatted = format_message(level, message, file, line, function);
        
        std::lock_guard<std::mutex> lock(mutex_);
        // 输出到控制台
        if (level >= LogLevel::WARN) {
            std::cerr << formatted << std::endl;
        } else {
            std::cout << formatted << std::endl;
        }
        
        // 输出到文件
        if (file_.is_open()) {
            file_ << formatted << std::endl;
        }
        
        // 调用回调函数
        for (auto& cb : callbacks_) {
            cb(level, formatted);
        }
    }

    /**
     * @class LogStream
     * @brief 流式日志记录器，支持类似std::cout的链式操作
     * 
     * 提供流式接口用于构建日志消息，在析构时自动输出完整的日志。
     * 支持各种数据类型的流式输入和标准流操纵符。
     */
    class LogStream {
    public:
        /**
         * @brief 构造函数，初始化流式日志记录器
         * @param logger Logger实例引用
         * @param level 日志级别
         * @param file 源文件名
         * @param line 源文件行号
         * @param function 函数名
         * 
         * 创建一个流式日志记录器实例，用于收集日志消息内容。
         */
        LogStream(Logger& logger, LogLevel level, 
                  const char* file, int line, const char* function)
            : logger_(logger), level_(level), 
              file_(file), line_(line), function_(function) {}
        
        /**
         * @brief 析构函数，输出收集到的日志消息
         * 
         * 在对象销毁时，如果消息缓冲区不为空，
         * 则调用Logger的log方法输出完整的日志消息。
         */
        ~LogStream() {
            if (!message_.str().empty()) {
                logger_.log(level_, message_.str(), file_, line_, function_);
            }
        }
        
        /**
         * @brief 流式输入操作符，支持任意类型的数据
         * @tparam T 输入数据的类型
         * @param value 要输入到日志流的值
         * @return LogStream引用，支持链式操作
         * 
         * 使用完美转发将各种类型的数据添加到日志消息缓冲区中。
         */
        template <typename T>
        LogStream& operator<<(T&& value) {
            message_ << std::forward<T>(value);
            return *this;
        }
        
        /**
         * @brief 流式操纵符支持
         * @param manip 标准流操纵符函数指针
         * @return LogStream引用，支持链式操作
         * 
         * 支持std::endl、std::flush等标准流操纵符的使用。
         */
        LogStream& operator<<(std::ostream& (*manip)(std::ostream&)) {
            manip(message_);
            return *this;
        }

    private:
        Logger& logger_;          ///< Logger实例引用
        LogLevel level_;          ///< 日志级别
        const char* file_;        ///< 源文件名
        int line_;                ///< 源文件行号
        const char* function_;    ///< 函数名
        std::ostringstream message_; ///< 消息缓冲区，用于收集日志内容
    };

private:
    /**
     * @brief 私有构造函数，实现单例模式
     * 
     * 将构造函数设为私有，防止外部直接创建实例。
     * 默认日志级别设置为INFO，默认启用时间戳输出，默认启用彩色输出，默认启用函数名和行号显示。
     */
    Logger() : level_(LogLevel::INFO), enable_timestamp_(true), enable_color_(true), enable_function_line_(true) {}
    
    /**
     * @brief 禁用拷贝构造函数
     * 
     * 单例模式不允许拷贝构造，确保全局唯一性。
     */
    Logger(const Logger&) = delete;
    
    /**
     * @brief 禁用赋值操作符
     * 
     * 单例模式不允许赋值操作，确保全局唯一性。
     */
    Logger& operator=(const Logger&) = delete;

    /**
     * @brief 格式化日志消息
     * @param level 日志级别
     * @param message 原始消息内容
     * @param file 源文件名（当前未使用）
     * @param line 源文件行号
     * @param function 函数名
     * @return 格式化后的完整日志消息字符串
     * 
     * 根据配置决定是否包含时间戳，格式化日志消息包含时间戳（可选）、
     * 日志级别、函数名、行号和消息内容。
     */
    std::string format_message(LogLevel level, const std::string& message, 
                              [[maybe_unused]]const char* file, int line, const char* function) {
        std::ostringstream ss;
        
        // 根据配置决定是否添加时间戳
        if (enable_timestamp_) {
            auto now = std::chrono::system_clock::now();
            auto now_time = std::chrono::system_clock::to_time_t(now);
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        now.time_since_epoch()) % 1000;
            
            ss << std::put_time(std::localtime(&now_time), "%Y-%m-%d %H:%M:%S")
               << '.' << std::setfill('0') << std::setw(3) << ms.count() << " ";
        }
        
        ss << "[" << level_to_string(level) << "] ";
        
        // 根据配置决定是否添加函数名和行号
        if (enable_function_line_) {
            ss << "[" << function << ":" << line << "] ";
        }
        
        ss << message;
        
        return ss.str();
    }

    /**
     * @brief 将日志级别转换为字符串
     * @param level 要转换的日志级别
     * @return 对应的字符串表示（根据颜色设置可能包含ANSI颜色代码）
     * 
     * 使用静态映射表将LogLevel枚举值转换为对应的字符串。
     * 如果启用了彩色输出，会为不同级别添加相应的颜色代码。
     * 如果级别未知，返回"UNKNOWN"。
     */
    std::string level_to_string(LogLevel level) {
        if (enable_color_) {
            // 彩色输出模式
            static const std::unordered_map<LogLevel, std::string> color_level_map = {
                {LogLevel::TRACE, std::string(ColorCodes::GRAY) + "TRACE" + ColorCodes::RESET},
                {LogLevel::DEBUG, std::string(ColorCodes::CYAN) + "DEBUG" + ColorCodes::RESET},
                {LogLevel::INFO,  std::string(ColorCodes::GREEN) + "INFO" + ColorCodes::RESET},
                {LogLevel::WARN,  std::string(ColorCodes::YELLOW) + "WARN" + ColorCodes::RESET},
                {LogLevel::ERROR, std::string(ColorCodes::RED) + "ERROR" + ColorCodes::RESET},
                {LogLevel::FATAL, std::string(ColorCodes::BOLD) + ColorCodes::MAGENTA + "FATAL" + ColorCodes::RESET}
            };
            auto it = color_level_map.find(level);
            return it != color_level_map.end() ? it->second : "UNKNOWN";
        } else {
            // 普通输出模式
            static const std::unordered_map<LogLevel, std::string> level_map = {
                {LogLevel::TRACE, "TRACE"},
                {LogLevel::DEBUG, "DEBUG"},
                {LogLevel::INFO,  "INFO"},
                {LogLevel::WARN,  "WARN"},
                {LogLevel::ERROR, "ERROR"},
                {LogLevel::FATAL, "FATAL"}
            };
            auto it = level_map.find(level);
            return it != level_map.end() ? it->second : "UNKNOWN";
        }
    }

    std::mutex mutex_;                    ///< 互斥锁，保证线程安全
    LogLevel level_;                      ///< 当前日志级别
    bool enable_timestamp_;               ///< 是否启用时间戳输出
    bool enable_color_;                   ///< 是否启用彩色输出
    bool enable_function_line_;           ///< 是否启用函数名和行号显示
    std::ofstream file_;                  ///< 日志文件输出流
    std::vector<OutputCallback> callbacks_; ///< 自定义输出回调函数列表
};

/**
 * @defgroup LogMacros 日志宏定义
 * @brief 提供便捷的日志记录宏，支持流式语法
 * 
 * 这些宏简化了日志记录的使用，自动捕获文件名、行号和函数名信息。
 * 使用方式类似于std::cout，支持链式操作。
 * 
 * @par 使用示例:
 * @code
 * LOG_INFO << "程序启动，版本号: " << version;
 * LOG_WARN << "内存使用率较高: " << usage << "%";
 * LOG_ERROR << "文件打开失败: " << filename;
 * @endcode
 * @{
 */

/**
 * @def LOG_TRACE
 * @brief 跟踪级别日志宏
 * 
 * 用于记录最详细的调试信息，通常在开发阶段使用。
 * 
 * @par 使用示例:
 * @code
 * LOG_TRACE << "进入函数 processData(), 参数count=" << count;
 * @endcode
 */
#define LOG_TRACE MB_DDF::Debug::Logger::LogStream(MB_DDF::Debug::Logger::instance(), MB_DDF::Debug::LogLevel::TRACE, __FILE__, __LINE__, __func__)

/**
 * @def LOG_DEBUG
 * @brief 调试级别日志宏
 * 
 * 用于记录调试信息，帮助开发者理解程序执行流程。
 * 
 * @par 使用示例:
 * @code
 * LOG_DEBUG << "处理数据包，大小: " << packet_size << " 字节";
 * @endcode
 */
#define LOG_DEBUG MB_DDF::Debug::Logger::LogStream(MB_DDF::Debug::Logger::instance(), MB_DDF::Debug::LogLevel::DEBUG, __FILE__, __LINE__, __func__)

/**
 * @def LOG_INFO
 * @brief 信息级别日志宏
 * 
 * 用于记录一般性的运行信息，如程序状态变化、重要操作等。
 * 
 * @par 使用示例:
 * @code
 * LOG_INFO << "服务器启动成功，监听端口: " << port;
 * @endcode
 */
#define LOG_INFO  MB_DDF::Debug::Logger::LogStream(MB_DDF::Debug::Logger::instance(), MB_DDF::Debug::LogLevel::INFO,  __FILE__, __LINE__, __func__)

/**
 * @def LOG_WARN
 * @brief 警告级别日志宏
 * 
 * 用于记录潜在的问题或异常情况，程序可以继续运行但需要注意。
 * 
 * @par 使用示例:
 * @code
 * LOG_WARN << "配置文件不存在，使用默认配置: " << default_config;
 * @endcode
 */
#define LOG_WARN  MB_DDF::Debug::Logger::LogStream(MB_DDF::Debug::Logger::instance(), MB_DDF::Debug::LogLevel::WARN,  __FILE__, __LINE__, __func__)

/**
 * @def LOG_ERROR
 * @brief 错误级别日志宏
 * 
 * 用于记录错误信息，表示发生了错误但程序可以继续运行。
 * 
 * @par 使用示例:
 * @code
 * LOG_ERROR << "数据库连接失败，错误码: " << error_code;
 * @endcode
 */
#define LOG_ERROR MB_DDF::Debug::Logger::LogStream(MB_DDF::Debug::Logger::instance(), MB_DDF::Debug::LogLevel::ERROR, __FILE__, __LINE__, __func__)

/**
 * @def LOG_FATAL
 * @brief 致命级别日志宏
 * 
 * 用于记录严重错误，通常导致程序无法继续运行。
 * 
 * @par 使用示例:
 * @code
 * LOG_FATAL << "内存分配失败，程序即将退出";
 * @endcode
 */
#define LOG_FATAL MB_DDF::Debug::Logger::LogStream(MB_DDF::Debug::Logger::instance(), MB_DDF::Debug::LogLevel::FATAL, __FILE__, __LINE__, __func__)

/**
 * @def LOG_IF
 * @brief 条件日志宏
 * @param level 日志级别
 * @param condition 条件表达式，为true时才记录日志
 * 
 * 只有当条件为真时才记录日志，避免不必要的日志输出。
 * 
 * @par 使用示例:
 * @code
 * LOG_IF(MB_DDF::Debug::LogLevel::DEBUG, count > 100) << "处理的数据量较大: " << count;
 * @endcode
 */
#define LOG_IF(level, condition) \
    !(condition) ? (void)0 : void(MB_DDF::Debug::Logger::LogStream( \
        MB_DDF::Debug::Logger::instance(), level, __FILE__, __LINE__, __func__))

/**
 * @defgroup LoggerConfigMacros Logger配置宏定义
 * @brief 提供便捷的Logger配置宏，简化常用设置操作
 * 
 * 这些宏简化了Logger的配置操作，避免重复输入长的命名空间和方法调用。
 * 
 * @par 使用示例:
 * @code
 * LOG_SET_LEVEL_DEBUG();        // 设置日志级别为DEBUG
 * LOG_SET_LEVEL_INFO();         // 设置日志级别为INFO
 * LOG_DISABLE_TIMESTAMP();      // 禁用时间戳输出
 * LOG_ENABLE_TIMESTAMP();       // 启用时间戳输出
 * @endcode
 * @{
 */

/**
 * @def LOG_SET_LEVEL_TRACE
 * @brief 设置日志级别为TRACE的宏
 */
#define LOG_SET_LEVEL_TRACE() MB_DDF::Debug::Logger::instance().set_level(MB_DDF::Debug::LogLevel::TRACE)

/**
 * @def LOG_SET_LEVEL_DEBUG
 * @brief 设置日志级别为DEBUG的宏
 */
#define LOG_SET_LEVEL_DEBUG() MB_DDF::Debug::Logger::instance().set_level(MB_DDF::Debug::LogLevel::DEBUG)

/**
 * @def LOG_SET_LEVEL_INFO
 * @brief 设置日志级别为INFO的宏
 */
#define LOG_SET_LEVEL_INFO() MB_DDF::Debug::Logger::instance().set_level(MB_DDF::Debug::LogLevel::INFO)

/**
 * @def LOG_SET_LEVEL_WARN
 * @brief 设置日志级别为WARN的宏
 */
#define LOG_SET_LEVEL_WARN() MB_DDF::Debug::Logger::instance().set_level(MB_DDF::Debug::LogLevel::WARN)

/**
 * @def LOG_SET_LEVEL_ERROR
 * @brief 设置日志级别为ERROR的宏
 */
#define LOG_SET_LEVEL_ERROR() MB_DDF::Debug::Logger::instance().set_level(MB_DDF::Debug::LogLevel::ERROR)

/**
 * @def LOG_SET_LEVEL_FATAL
 * @brief 设置日志级别为FATAL的宏
 */
#define LOG_SET_LEVEL_FATAL() MB_DDF::Debug::Logger::instance().set_level(MB_DDF::Debug::LogLevel::FATAL)

/**
 * @def LOG_SET_LEVEL_OFF
 * @brief 设置日志级别为OFF（关闭所有日志）的宏
 */
#define LOG_SET_LEVEL_OFF() MB_DDF::Debug::Logger::instance().set_level(MB_DDF::Debug::LogLevel::OFF)

/**
 * @def LOG_ENABLE_TIMESTAMP
 * @brief 启用时间戳输出的宏
 */
#define LOG_ENABLE_TIMESTAMP() MB_DDF::Debug::Logger::instance().set_timestamp_enabled(true)

/**
 * @def LOG_DISABLE_TIMESTAMP
 * @brief 禁用时间戳输出的宏
 */
#define LOG_DISABLE_TIMESTAMP() MB_DDF::Debug::Logger::instance().set_timestamp_enabled(false)

/**
 * @def LOG_ENABLE_COLOR
 * @brief 启用彩色输出宏
 * 
 * 调用此宏将启用日志输出的彩色显示，不同级别的日志将以不同颜色显示。
 * 
 * @par 颜色映射:
 * - TRACE: 灰色
 * - DEBUG: 青色
 * - INFO:  绿色
 * - WARN:  黄色
 * - ERROR: 红色
 * - FATAL: 粗体洋红色
 */
#define LOG_ENABLE_COLOR() MB_DDF::Debug::Logger::instance().set_color_enabled(true)

/**
 * @def LOG_DISABLE_COLOR
 * @brief 禁用彩色输出宏
 * 
 * 禁用日志输出中的ANSI颜色代码，输出纯文本格式。
 * 适用于不支持颜色的终端或需要纯文本日志的场景。
 */
#define LOG_DISABLE_COLOR() MB_DDF::Debug::Logger::instance().set_color_enabled(false)

/**
 * @def LOG_ENABLE_FUNCTION_LINE
 * @brief 启用函数名和行号显示宏
 * 
 * 启用日志输出中的[函数名:行号]信息显示。
 * 有助于调试时快速定位日志输出的源代码位置。
 */
#define LOG_ENABLE_FUNCTION_LINE() MB_DDF::Debug::Logger::instance().set_function_line_enabled(true)

/**
 * @def LOG_DISABLE_FUNCTION_LINE
 * @brief 禁用函数名和行号显示宏
 * 
 * 禁用日志输出中的[函数名:行号]信息显示。
 * 可以使日志输出更简洁，适用于生产环境或不需要详细位置信息的场景。
 */
#define LOG_DISABLE_FUNCTION_LINE() MB_DDF::Debug::Logger::instance().set_function_line_enabled(false)

/** @} */ // end of LoggerConfigMacros group

/** @} */ // end of LogMacros group

} // namespace Debug
} // namespace MB_DDF
