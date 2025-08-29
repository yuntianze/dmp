/**
 * @file logger.cpp
 * @brief Implementation of unified logging system
 * @author Stan Jiang
 * @date 2025-08-28
 */

#include "utils/logger.hpp"
#include "common/config.hpp"
#include <spdlog/cfg/env.h>
#include <toml++/toml.h>
#include <filesystem>
#include <iostream>

namespace dmp {

// TraceContext implementation
thread_local std::string TraceContext::current_trace_id_;
std::random_device TraceContext::rd_;
std::mt19937_64 TraceContext::gen_{TraceContext::rd_()};

std::string TraceContext::get_trace_id() {
    return current_trace_id_;
}

void TraceContext::set_trace_id(const std::string& trace_id) {
    current_trace_id_ = trace_id;
}

std::string TraceContext::generate_trace_id() {
    // Generate 128-bit trace ID (32 hex characters)
    std::uniform_int_distribution<uint64_t> dis;
    uint64_t high = dis(gen_);
    uint64_t low = dis(gen_);
    
    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::setw(16) << high 
        << std::setw(16) << low;
    
    std::string trace_id = oss.str();
    set_trace_id(trace_id);
    return trace_id;
}

void TraceContext::clear_trace_id() {
    current_trace_id_.clear();
}

// TraceScope implementation
TraceScope::TraceScope(const std::string& trace_id) 
    : trace_id_(trace_id), previous_trace_id_(TraceContext::get_trace_id()) {
    TraceContext::set_trace_id(trace_id_);
}

TraceScope::TraceScope() 
    : trace_id_(TraceContext::generate_trace_id()), 
      previous_trace_id_(TraceContext::get_trace_id()) {
    // trace_id_ is already set by generate_trace_id()
}

TraceScope::~TraceScope() {
    TraceContext::set_trace_id(previous_trace_id_);
}

// Logger implementation
bool Logger::initialized_ = false;
std::shared_ptr<spdlog::logger> Logger::default_logger_;
std::string Logger::log_pattern_ = "[%Y-%m-%d %H:%M:%S.%f] [%l] [%s:%#] [%!] %v";

bool Logger::initialize(const std::string& config_path) {
    if (initialized_) {
        return true;
    }
    
    try {
        // Load configuration
        if (!load_config(config_path)) {
            std::cerr << "Failed to load logging configuration from: " << config_path << std::endl;
            // Use default configuration
        }
        
        // Setup async logging for better performance
        setup_async_logging();
        
        // Setup sinks
        setup_sinks();
        
        // Set environment variable for default level
        spdlog::cfg::load_env_levels();
        
        initialized_ = true;
        
        LOG_INFO("DMP logging system initialized successfully");
        LOG_INFO("Log pattern: {}", log_pattern_);
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize logging system: " << e.what() << std::endl;
        return false;
    }
}

void Logger::shutdown() {
    if (!initialized_) {
        return;
    }
    
    LOG_INFO("Shutting down DMP logging system");
    flush_all();
    
    // Shutdown async logging
    spdlog::shutdown();
    
    initialized_ = false;
}

std::shared_ptr<spdlog::logger> Logger::get_logger() {
    if (!initialized_) {
        // Create a simple console logger as fallback
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        auto logger = std::make_shared<spdlog::logger>("default", console_sink);
        logger->set_pattern(log_pattern_);
        return logger;
    }
    
    return default_logger_;
}

std::shared_ptr<spdlog::logger> Logger::get_logger(const std::string& name) {
    auto logger = spdlog::get(name);
    if (!logger) {
        // Create new logger with same sinks as default
        if (default_logger_) {
            logger = std::make_shared<spdlog::logger>(name, default_logger_->sinks().begin(), 
                                                    default_logger_->sinks().end());
            logger->set_pattern(log_pattern_);
            logger->set_level(default_logger_->level());
            spdlog::register_logger(logger);
        } else {
            // Fallback to console logger
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            logger = std::make_shared<spdlog::logger>(name, console_sink);
            logger->set_pattern(log_pattern_);
            spdlog::register_logger(logger);
        }
    }
    return logger;
}

void Logger::set_level(spdlog::level::level_enum level) {
    spdlog::set_level(level);
    if (default_logger_) {
        default_logger_->set_level(level);
    }
}

void Logger::flush_all() {
    spdlog::apply_all([](std::shared_ptr<spdlog::logger> logger) {
        logger->flush();
    });
}

bool Logger::load_config(const std::string& config_path) {
    try {
        if (!std::filesystem::exists(config_path)) {
            std::cerr << "Logging config file not found: " << config_path << std::endl;
            return false;
        }
        
        auto config = toml::parse_file(config_path);
        
        // Load logging section
        if (auto logging_section = config["logging"]) {
            if (auto pattern = logging_section["pattern"].value<std::string>()) {
                log_pattern_ = *pattern;
            }
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error parsing logging config: " << e.what() << std::endl;
        return false;
    }
}

void Logger::setup_sinks() {
    std::vector<spdlog::sink_ptr> sinks;
    
    try {
        // Console sink with colors
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%f] [%^%l%$] [%s:%#] %v");
        sinks.push_back(console_sink);
        
        // Ensure logs directory exists
        std::filesystem::create_directories("logs");
        
        // Main rotating file sink
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            "logs/dmp_server.log", 1024 * 1024 * 100, 10); // 100MB, 10 files
        file_sink->set_pattern(log_pattern_);
        sinks.push_back(file_sink);
        
        // Error file sink (ERROR and FATAL only)
        auto error_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            "logs/dmp_error.log", 1024 * 1024 * 50, 5); // 50MB, 5 files
        error_sink->set_pattern(log_pattern_);
        error_sink->set_level(spdlog::level::err);
        sinks.push_back(error_sink);
        
        // Create default logger
        default_logger_ = std::make_shared<spdlog::logger>("dmp_default", sinks.begin(), sinks.end());
        default_logger_->set_pattern(log_pattern_);
        default_logger_->set_level(spdlog::level::info);
        default_logger_->flush_on(spdlog::level::err);
        
        // Register as default
        spdlog::set_default_logger(default_logger_);
        
        // Create audit logger
        auto audit_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(
            "logs/dmp_audit.log", 0, 0); // Roll at midnight
        audit_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%f] [AUDIT] [%s:%#] [%!] [%t] %v");
        
        auto audit_logger = std::make_shared<spdlog::logger>("audit", audit_sink);
        audit_logger->set_level(spdlog::level::info);
        spdlog::register_logger(audit_logger);
        
        // Create performance logger
        auto perf_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            "logs/dmp_performance.log", 1024 * 1024 * 50, 3); // 50MB, 3 files
        perf_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%f] [PERF] [%s:%#] [%!] %v");
        
        auto perf_logger = std::make_shared<spdlog::logger>("performance", perf_sink);
        perf_logger->set_level(spdlog::level::info);
        spdlog::register_logger(perf_logger);
        
    } catch (const std::exception& e) {
        std::cerr << "Error setting up sinks: " << e.what() << std::endl;
        throw;
    }
}

void Logger::setup_async_logging() {
    try {
        // Initialize async logging with queue size and thread count
        spdlog::init_thread_pool(8192, 1);
        
        // Set overflow policy to block (safer for important logs)
        // Alternative: spdlog::async_overflow_policy::overrun_oldest
        
    } catch (const std::exception& e) {
        std::cerr << "Error setting up async logging: " << e.what() << std::endl;
        // Continue with synchronous logging
    }
}

} // namespace dmp
