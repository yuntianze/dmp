/**
 * @file logger.hpp
 * @brief Unified logging system for DMP risk control system
 * @author Stan Jiang
 * @date 2025-08-28
 */
#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/async.h>
#include <memory>
#include <string>
#include <thread>
#include <random>
#include <sstream>
#include <iomanip>
#include <chrono>

namespace dmp {

/**
 * @brief Thread-local trace ID generator and storage
 */
class TraceContext {
public:
    /**
     * @brief Get current thread's trace ID
     * @return Current trace ID as string
     */
    static std::string get_trace_id();
    
    /**
     * @brief Set current thread's trace ID
     * @param trace_id New trace ID
     */
    static void set_trace_id(const std::string& trace_id);
    
    /**
     * @brief Generate new trace ID for current thread
     * @return Generated trace ID
     */
    static std::string generate_trace_id();
    
    /**
     * @brief Clear current thread's trace ID
     */
    static void clear_trace_id();

private:
    static thread_local std::string current_trace_id_;
    static std::random_device rd_;
    static std::mt19937_64 gen_;
};

/**
 * @brief RAII trace ID scope manager
 */
class TraceScope {
public:
    explicit TraceScope(const std::string& trace_id);
    explicit TraceScope(); // Generate new trace ID
    ~TraceScope();
    
    const std::string& get_trace_id() const { return trace_id_; }

private:
    std::string trace_id_;
    std::string previous_trace_id_;
};

/**
 * @brief Unified logger configuration and management
 */
class Logger {
public:
    /**
     * @brief Initialize logging system
     * @param config_path Path to logging configuration file
     * @return true if initialization successful
     */
    static bool initialize(const std::string& config_path = "config/logging.toml");
    
    /**
     * @brief Shutdown logging system
     */
    static void shutdown();
    
    /**
     * @brief Get default logger instance
     * @return Shared pointer to logger
     */
    static std::shared_ptr<spdlog::logger> get_logger();
    
    /**
     * @brief Get named logger instance
     * @param name Logger name
     * @return Shared pointer to logger
     */
    static std::shared_ptr<spdlog::logger> get_logger(const std::string& name);
    
    /**
     * @brief Set global log level
     * @param level Log level (FATAL=0, ERROR=1, INFO=2, DEBUG=3)
     */
    static void set_level(spdlog::level::level_enum level);
    
    /**
     * @brief Flush all loggers
     */
    static void flush_all();

private:
    static bool initialized_;
    static std::shared_ptr<spdlog::logger> default_logger_;
    static std::string log_pattern_;
    
    static bool load_config(const std::string& config_path);
    static void setup_sinks();
    static void setup_async_logging();
};

} // namespace dmp

// Enhanced logging macros with trace ID support
#define DMP_LOG_WITH_LOCATION(logger, level, fmt_str, ...) \
    logger->log(spdlog::source_loc{__FILE__, __LINE__, __FUNCTION__}, level, fmt_str, ##__VA_ARGS__)

// Main logging macros
#define LOG_FATAL(fmt_str, ...) \
    do { \
        auto trace_id = dmp::TraceContext::get_trace_id(); \
        if (!trace_id.empty()) { \
            DMP_LOG_WITH_LOCATION(dmp::Logger::get_logger(), spdlog::level::critical, "[{}] " fmt_str, trace_id, ##__VA_ARGS__); \
        } else { \
            DMP_LOG_WITH_LOCATION(dmp::Logger::get_logger(), spdlog::level::critical, fmt_str, ##__VA_ARGS__); \
        } \
    } while(0)

#define LOG_ERROR(fmt_str, ...) \
    do { \
        auto trace_id = dmp::TraceContext::get_trace_id(); \
        if (!trace_id.empty()) { \
            DMP_LOG_WITH_LOCATION(dmp::Logger::get_logger(), spdlog::level::err, "[{}] " fmt_str, trace_id, ##__VA_ARGS__); \
        } else { \
            DMP_LOG_WITH_LOCATION(dmp::Logger::get_logger(), spdlog::level::err, fmt_str, ##__VA_ARGS__); \
        } \
    } while(0)

#define LOG_INFO(fmt_str, ...) \
    do { \
        auto trace_id = dmp::TraceContext::get_trace_id(); \
        if (!trace_id.empty()) { \
            DMP_LOG_WITH_LOCATION(dmp::Logger::get_logger(), spdlog::level::info, "[{}] " fmt_str, trace_id, ##__VA_ARGS__); \
        } else { \
            DMP_LOG_WITH_LOCATION(dmp::Logger::get_logger(), spdlog::level::info, fmt_str, ##__VA_ARGS__); \
        } \
    } while(0)

#define LOG_DEBUG(fmt_str, ...) \
    do { \
        auto trace_id = dmp::TraceContext::get_trace_id(); \
        if (!trace_id.empty()) { \
            DMP_LOG_WITH_LOCATION(dmp::Logger::get_logger(), spdlog::level::debug, "[{}] " fmt_str, trace_id, ##__VA_ARGS__); \
        } else { \
            DMP_LOG_WITH_LOCATION(dmp::Logger::get_logger(), spdlog::level::debug, fmt_str, ##__VA_ARGS__); \
        } \
    } while(0)

// Named logger macros
#define LOG_NAMED_FATAL(name, fmt_str, ...) DMP_LOG_WITH_LOCATION(dmp::Logger::get_logger(name), spdlog::level::critical, fmt_str, ##__VA_ARGS__)
#define LOG_NAMED_ERROR(name, fmt_str, ...) DMP_LOG_WITH_LOCATION(dmp::Logger::get_logger(name), spdlog::level::err, fmt_str, ##__VA_ARGS__)
#define LOG_NAMED_INFO(name, fmt_str, ...) DMP_LOG_WITH_LOCATION(dmp::Logger::get_logger(name), spdlog::level::info, fmt_str, ##__VA_ARGS__)
#define LOG_NAMED_DEBUG(name, fmt_str, ...) DMP_LOG_WITH_LOCATION(dmp::Logger::get_logger(name), spdlog::level::debug, fmt_str, ##__VA_ARGS__)

// Audit logging (special logger for compliance)
#define LOG_AUDIT(fmt_str, ...) DMP_LOG_WITH_LOCATION(dmp::Logger::get_logger("audit"), spdlog::level::info, fmt_str, ##__VA_ARGS__)

// Performance logging
#define LOG_PERF(fmt_str, ...) DMP_LOG_WITH_LOCATION(dmp::Logger::get_logger("performance"), spdlog::level::info, fmt_str, ##__VA_ARGS__)

// Conditional logging
#define LOG_ERROR_IF(condition, fmt_str, ...) \
    do { if (condition) LOG_ERROR(fmt_str, ##__VA_ARGS__); } while(0)

#define LOG_INFO_IF(condition, fmt_str, ...) \
    do { if (condition) LOG_INFO(fmt_str, ##__VA_ARGS__); } while(0)

#define LOG_DEBUG_IF(condition, fmt_str, ...) \
    do { if (condition) LOG_DEBUG(fmt_str, ##__VA_ARGS__); } while(0)

// Convenience macros for function entry/exit
#define LOG_FUNCTION_ENTRY() LOG_DEBUG("Entering function: {}", __FUNCTION__)
#define LOG_FUNCTION_EXIT() LOG_DEBUG("Exiting function: {}", __FUNCTION__)

// Performance measurement macro
#define LOG_DURATION(name, code_block) \
    do { \
        auto start_time = std::chrono::high_resolution_clock::now(); \
        code_block; \
        auto end_time = std::chrono::high_resolution_clock::now(); \
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time); \
        LOG_PERF("{} took {} microseconds", name, duration.count()); \
    } while(0)
