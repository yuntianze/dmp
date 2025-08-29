/**
 * @file config.hpp
 * @brief Configuration management system for DMP risk control system
 * @author Stan Jiang
 * @date 2025-08-28
 */
#pragma once

#include "common/types.hpp"
#include <toml++/toml.h>
#include <string>
#include <chrono>
#include <memory>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <functional>
#include <set>

namespace dmp {

/**
 * @brief Server configuration parameters
 * 
 * Contains all HTTP server related settings including
 * performance tuning and resource limits.
 */
struct ServerConfig {
    std::string host = "0.0.0.0";
    uint16_t port = 8080;
    uint32_t threads = 8;
    uint32_t keep_alive_timeout = 60;
    uint32_t max_connections = 10000;
    
    // Performance targets (SLO)
    float target_p99_ms = 50.0f;
    uint32_t target_qps = 10000;
    uint32_t max_memory_gb = 4;
    uint32_t max_cpu_percent = 80;
    
    /**
     * @brief Load server config from TOML table
     * @param table TOML table containing server settings
     * @return Result with config or error details
     */
    static Result<ServerConfig> from_toml(const toml::table& table);
    
    /**
     * @brief Validate server configuration
     * @return true if configuration is valid
     */
    bool is_valid() const;
};

/**
 * @brief Feature extraction and caching configuration
 */
struct FeatureConfig {
    bool enable_cache = true;
    uint32_t cache_size_mb = 512;
    uint32_t cache_ttl_seconds = 300;
    
    // L1 cache (thread-local)
    uint32_t l1_size_mb = 16;
    uint32_t l1_ttl_seconds = 60;
    
    // L2 cache (process-shared)
    uint32_t l2_size_mb = 256;
    uint32_t l2_ttl_seconds = 300;
    
    // L3 cache (Redis)
    bool enable_redis = false;
    std::string redis_host = "localhost";
    uint16_t redis_port = 6379;
    uint32_t l3_size_mb = 1024;
    uint32_t l3_ttl_seconds = 3600;
    
    static Result<FeatureConfig> from_toml(const toml::table& table);
    bool is_valid() const;
};

/**
 * @brief Logging system configuration
 */
struct LoggingConfig {
    std::string level = "info";
    std::string file_path = "/var/log/dmp/server.log";
    uint32_t max_size_mb = 100;
    uint32_t max_files = 10;
    bool enable_console = true;
    bool enable_file = true;
    
    static Result<LoggingConfig> from_toml(const toml::table& table);
    bool is_valid() const;
};

/**
 * @brief Monitoring and metrics configuration
 */
struct MonitoringConfig {
    bool enable_prometheus = true;
    uint16_t prometheus_port = 9090;
    uint32_t metrics_interval_seconds = 1;
    std::string metrics_path = "/metrics";
    
    static Result<MonitoringConfig> from_toml(const toml::table& table);
    bool is_valid() const;
};

/**
 * @brief Complete system configuration
 * 
 * Aggregates all configuration sections with support for
 * hot reloading and validation. Thread-safe for concurrent access.
 */
class SystemConfig {
public:
    /**
     * @brief Load configuration from TOML file
     * @param config_path Path to TOML configuration file
     * @return Result with loaded config or error details
     * 
     * Performance: Target < 10ms for typical config file
     * Thread-safe: Yes, uses internal synchronization
     */
    static Result<std::shared_ptr<SystemConfig>> load_from_file(const std::string& config_path);
    
    /**
     * @brief Load configuration from TOML string
     * @param toml_content TOML configuration content
     * @return Result with loaded config or error details
     */
    static Result<std::shared_ptr<SystemConfig>> load_from_string(const std::string& toml_content);
    
    /**
     * @brief Enable automatic hot reloading of configuration
     * @param check_interval_ms File modification check interval
     * @param callback Optional callback for config changes
     * 
     * Monitors configuration file for changes and automatically
     * reloads when modifications are detected.
     */
    void enable_hot_reload(uint32_t check_interval_ms = 5000,
                          std::function<void(const SystemConfig&)> callback = nullptr);
    
    /**
     * @brief Disable hot reloading
     */
    void disable_hot_reload();
    
    /**
     * @brief Force reload configuration from file
     * @return Result indicating success or failure
     */
    Result<void> reload();
    
    /**
     * @brief Get server configuration (thread-safe)
     * @return Server configuration copy
     */
    ServerConfig get_server_config() const;
    
    /**
     * @brief Get feature configuration (thread-safe)
     * @return Feature configuration copy
     */
    FeatureConfig get_feature_config() const;
    
    /**
     * @brief Get logging configuration (thread-safe)
     * @return Logging configuration copy
     */
    LoggingConfig get_logging_config() const;
    
    /**
     * @brief Get monitoring configuration (thread-safe)
     * @return Monitoring configuration copy
     */
    MonitoringConfig get_monitoring_config() const;
    
    /**
     * @brief Check if configuration is valid
     * @return true if all sections are valid
     */
    bool is_valid() const;
    
    /**
     * @brief Get configuration file path
     * @return Path to loaded configuration file
     */
    std::string get_config_path() const;
    
    /**
     * @brief Get last modification time of config file
     * @return Timestamp of last modification
     */
    std::chrono::system_clock::time_point get_last_modified() const;

private:
    SystemConfig() = default;
    
    /**
     * @brief Load and parse TOML configuration
     * @param table Parsed TOML table
     * @return Result indicating success or failure
     */
    Result<void> load_from_toml(const toml::table& table);
    
    /**
     * @brief Check if configuration file has been modified
     * @return true if file was modified since last load
     */
    bool is_file_modified() const;
    
    /**
     * @brief Background thread function for hot reloading
     */
    void hot_reload_worker();

    // Configuration sections
    mutable std::shared_mutex config_mutex_;
    ServerConfig server_config_;
    FeatureConfig feature_config_;
    LoggingConfig logging_config_;
    MonitoringConfig monitoring_config_;
    
    // File monitoring for hot reload
    std::string config_file_path_;
    std::chrono::system_clock::time_point last_modified_;
    std::atomic<bool> hot_reload_enabled_{false};
    std::atomic<bool> hot_reload_stop_flag_{false};
    std::unique_ptr<std::thread> hot_reload_thread_;
    uint32_t hot_reload_interval_ms_ = 5000;
    std::function<void(const SystemConfig&)> hot_reload_callback_;
    
public:
    // Singleton instance for global access
    static std::shared_ptr<SystemConfig> instance_;
    static std::mutex instance_mutex_;
};

/**
 * @brief Get global system configuration instance
 * @return Shared pointer to global config instance
 * 
 * Returns the global configuration instance. Must be initialized
 * with SystemConfig::load_from_file() before first use.
 */
std::shared_ptr<SystemConfig> get_system_config();

/**
 * @brief Set global system configuration instance
 * @param config Shared pointer to config instance
 * 
 * Sets the global configuration instance for application use.
 * Thread-safe operation.
 */
void set_system_config(std::shared_ptr<SystemConfig> config);

} // namespace dmp
