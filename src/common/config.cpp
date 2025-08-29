#include "common/config.hpp"
#include "utils/logger.hpp"
#include <fstream>
#include <filesystem>
#include <thread>
#include <regex>
#include <iostream>
#include <iomanip>

namespace dmp {

namespace {
    /**
     * @brief Helper to safely extract integer from TOML
     */
    template<typename T>
    T extract_integer(const toml::table& table, const std::string& key, T default_value) {
        if (auto value = table[key].value<int64_t>()) {
            return static_cast<T>(*value);
        }
        return default_value;
    }
    
    /**
     * @brief Helper to safely extract string from TOML
     */
    std::string extract_string(const toml::table& table, const std::string& key, 
                              const std::string& default_value) {
        if (auto value = table[key].value<std::string>()) {
            return *value;
        }
        return default_value;
    }
    
    /**
     * @brief Helper to safely extract boolean from TOML
     */
    bool extract_bool(const toml::table& table, const std::string& key, bool default_value) {
        if (auto value = table[key].value<bool>()) {
            return *value;
        }
        return default_value;
    }
    
    /**
     * @brief Helper to safely extract double from TOML
     */
    double extract_double(const toml::table& table, const std::string& key, double default_value) {
        if (auto value = table[key].value<double>()) {
            return *value;
        }
        return default_value;
    }
    
    /**
     * @brief Validate port number range
     */
    bool is_valid_port(uint16_t port) {
        return port > 0 && port <= 65535;
    }
    
    /**
     * @brief Validate log level string
     */
    bool is_valid_log_level(const std::string& level) {
        static const std::set<std::string> valid_levels = {
            "trace", "debug", "info", "warn", "error", "critical", "off"
        };
        return valid_levels.find(level) != valid_levels.end();
    }
}

// ServerConfig implementation
Result<ServerConfig> ServerConfig::from_toml(const toml::table& table) {
    ServerConfig config;
    
    try {
        // Extract server section
        if (auto server_table = table["server"].as_table()) {
            config.host = extract_string(*server_table, "host", config.host);
            config.port = extract_integer(*server_table, "port", config.port);
            config.threads = extract_integer(*server_table, "threads", config.threads);
            config.keep_alive_timeout = extract_integer(*server_table, "keep_alive_timeout", 
                                                       config.keep_alive_timeout);
            config.max_connections = extract_integer(*server_table, "max_connections", 
                                                    config.max_connections);
        }
        
        // Extract performance section
        if (auto perf_table = table["performance"].as_table()) {
            config.target_p99_ms = static_cast<float>(extract_double(*perf_table, "target_p99_ms", 
                                                                    config.target_p99_ms));
            config.target_qps = extract_integer(*perf_table, "target_qps", config.target_qps);
            config.max_memory_gb = extract_integer(*perf_table, "max_memory_gb", 
                                                  config.max_memory_gb);
            config.max_cpu_percent = extract_integer(*perf_table, "max_cpu_percent", 
                                                    config.max_cpu_percent);
        }
        
    } catch (const toml::parse_error& e) {
        return {config, ErrorCode::INVALID_JSON_FORMAT, 
               std::string("TOML parsing error: ") + e.what()};
    }
    
    if (!config.is_valid()) {
        return {config, ErrorCode::INVALID_REQUEST, "Invalid server configuration values"};
    }
    
    return {config, ErrorCode::SUCCESS, ""};
}

bool ServerConfig::is_valid() const {
    return !host.empty() &&
           is_valid_port(port) &&
           threads > 0 && threads <= 64 &&
           keep_alive_timeout > 0 && keep_alive_timeout <= 3600 &&
           max_connections > 0 && max_connections <= 100000 &&
           target_p99_ms > 0.0f && target_p99_ms <= 10000.0f &&
           target_qps > 0 && target_qps <= 1000000 &&
           max_memory_gb > 0 && max_memory_gb <= 128 &&
           max_cpu_percent > 0 && max_cpu_percent <= 100;
}

// FeatureConfig implementation
Result<FeatureConfig> FeatureConfig::from_toml(const toml::table& table) {
    FeatureConfig config;
    
    try {
        // Extract features section
        if (auto features_table = table["features"].as_table()) {
            config.enable_cache = extract_bool(*features_table, "enable_cache", config.enable_cache);
            config.cache_size_mb = extract_integer(*features_table, "cache_size_mb", 
                                                  config.cache_size_mb);
            config.cache_ttl_seconds = extract_integer(*features_table, "cache_ttl_seconds", 
                                                      config.cache_ttl_seconds);
        }
        
        // Extract cache configuration if available (from features.yaml structure)
        if (auto cache_table = table["cache_config"].as_table()) {
            if (auto levels_table = (*cache_table)["levels"].as_table()) {
                // L1 cache config
                if (auto l1_table = (*levels_table)["l1_thread_local"].as_table()) {
                    config.l1_size_mb = extract_integer(*l1_table, "size_mb", config.l1_size_mb);
                    config.l1_ttl_seconds = extract_integer(*l1_table, "ttl_seconds", 
                                                           config.l1_ttl_seconds);
                }
                
                // L2 cache config
                if (auto l2_table = (*levels_table)["l2_process_shared"].as_table()) {
                    config.l2_size_mb = extract_integer(*l2_table, "size_mb", config.l2_size_mb);
                    config.l2_ttl_seconds = extract_integer(*l2_table, "ttl_seconds", 
                                                           config.l2_ttl_seconds);
                }
                
                // L3 cache config (Redis)
                if (auto l3_table = (*levels_table)["l3_redis"].as_table()) {
                    config.l3_size_mb = extract_integer(*l3_table, "size_mb", config.l3_size_mb);
                    config.l3_ttl_seconds = extract_integer(*l3_table, "ttl_seconds", 
                                                           config.l3_ttl_seconds);
                    config.redis_host = extract_string(*l3_table, "host", config.redis_host);
                    config.redis_port = extract_integer(*l3_table, "port", config.redis_port);
                    config.enable_redis = true; // Enable if Redis config is present
                }
            }
        }
        
    } catch (const toml::parse_error& e) {
        return {config, ErrorCode::INVALID_JSON_FORMAT, 
               std::string("TOML parsing error: ") + e.what()};
    }
    
    if (!config.is_valid()) {
        return {config, ErrorCode::INVALID_REQUEST, "Invalid feature configuration values"};
    }
    
    return {config, ErrorCode::SUCCESS, ""};
}

bool FeatureConfig::is_valid() const {
    return cache_size_mb > 0 && cache_size_mb <= 16384 && // Max 16GB
           cache_ttl_seconds > 0 && cache_ttl_seconds <= 86400 && // Max 24h
           l1_size_mb > 0 && l1_size_mb <= 1024 && // Max 1GB
           l1_ttl_seconds > 0 && l1_ttl_seconds <= 3600 && // Max 1h
           l2_size_mb > 0 && l2_size_mb <= 4096 && // Max 4GB
           l2_ttl_seconds > 0 && l2_ttl_seconds <= 7200 && // Max 2h
           l3_size_mb > 0 && l3_size_mb <= 32768 && // Max 32GB
           l3_ttl_seconds > 0 && l3_ttl_seconds <= 86400 && // Max 24h
           (!enable_redis || is_valid_port(redis_port));
}

// LoggingConfig implementation
Result<LoggingConfig> LoggingConfig::from_toml(const toml::table& table) {
    LoggingConfig config;
    
    try {
        if (auto logging_table = table["logging"].as_table()) {
            config.level = extract_string(*logging_table, "level", config.level);
            config.file_path = extract_string(*logging_table, "file", config.file_path);
            config.max_size_mb = extract_integer(*logging_table, "max_size_mb", config.max_size_mb);
            config.max_files = extract_integer(*logging_table, "max_files", config.max_files);
            config.enable_console = extract_bool(*logging_table, "enable_console", 
                                               config.enable_console);
            config.enable_file = extract_bool(*logging_table, "enable_file", config.enable_file);
        }
    } catch (const toml::parse_error& e) {
        return {config, ErrorCode::INVALID_JSON_FORMAT, 
               std::string("TOML parsing error: ") + e.what()};
    }
    
    if (!config.is_valid()) {
        return {config, ErrorCode::INVALID_REQUEST, "Invalid logging configuration values"};
    }
    
    return {config, ErrorCode::SUCCESS, ""};
}

bool LoggingConfig::is_valid() const {
    return is_valid_log_level(level) &&
           !file_path.empty() &&
           max_size_mb > 0 && max_size_mb <= 1024 && // Max 1GB per file
           max_files > 0 && max_files <= 100;
}

// MonitoringConfig implementation
Result<MonitoringConfig> MonitoringConfig::from_toml(const toml::table& table) {
    MonitoringConfig config;
    
    try {
        if (auto monitoring_table = table["monitoring"].as_table()) {
            config.enable_prometheus = extract_bool(*monitoring_table, "enable_prometheus", 
                                                   config.enable_prometheus);
            config.prometheus_port = extract_integer(*monitoring_table, "prometheus_port", 
                                                    config.prometheus_port);
            config.metrics_interval_seconds = extract_integer(*monitoring_table, 
                                                             "metrics_interval_seconds", 
                                                             config.metrics_interval_seconds);
            config.metrics_path = extract_string(*monitoring_table, "metrics_path", 
                                                config.metrics_path);
        }
    } catch (const toml::parse_error& e) {
        return {config, ErrorCode::INVALID_JSON_FORMAT, 
               std::string("TOML parsing error: ") + e.what()};
    }
    
    if (!config.is_valid()) {
        return {config, ErrorCode::INVALID_REQUEST, "Invalid monitoring configuration values"};
    }
    
    return {config, ErrorCode::SUCCESS, ""};
}

bool MonitoringConfig::is_valid() const {
    return is_valid_port(prometheus_port) &&
           metrics_interval_seconds > 0 && metrics_interval_seconds <= 3600 &&
           !metrics_path.empty() && metrics_path[0] == '/';
}

// SystemConfig static members
std::shared_ptr<SystemConfig> SystemConfig::instance_;
std::mutex SystemConfig::instance_mutex_;

// SystemConfig implementation
Result<std::shared_ptr<SystemConfig>> SystemConfig::load_from_file(const std::string& config_path) {
    try {
        if (!std::filesystem::exists(config_path)) {
            return {nullptr, ErrorCode::INVALID_REQUEST, 
                   "Configuration file does not exist: " + config_path};
        }
        
        auto config = std::shared_ptr<SystemConfig>(new SystemConfig());
        config->config_file_path_ = config_path;
        
        // Get file modification time
        auto file_time = std::filesystem::last_write_time(config_path);
        config->last_modified_ = std::chrono::system_clock::from_time_t(
            std::chrono::duration_cast<std::chrono::seconds>(
                file_time.time_since_epoch()).count());
        
        // Parse TOML file
        toml::table toml_table;
        try {
            toml_table = toml::parse_file(config_path);
        } catch (const toml::parse_error& e) {
            return {nullptr, ErrorCode::INVALID_JSON_FORMAT, 
                   std::string("TOML parsing failed: ") + e.what()};
        }
        
        auto load_result = config->load_from_toml(toml_table);
        if (load_result.is_error()) {
            return {nullptr, load_result.error_code, load_result.error_message};
        }
        
        LOG_INFO("Configuration loaded successfully from: {}", config_path);
        return {config, ErrorCode::SUCCESS, ""};
        
    } catch (const std::exception& e) {
        return {nullptr, ErrorCode::INTERNAL_ERROR, 
               std::string("Failed to load configuration: ") + e.what()};
    }
}

Result<std::shared_ptr<SystemConfig>> SystemConfig::load_from_string(const std::string& toml_content) {
    try {
        auto config = std::shared_ptr<SystemConfig>(new SystemConfig());
        
        toml::table toml_table;
        try {
            toml_table = toml::parse(toml_content);
        } catch (const toml::parse_error& e) {
            return {nullptr, ErrorCode::INVALID_JSON_FORMAT, 
                   std::string("TOML parsing failed: ") + e.what()};
        }
        
        auto load_result = config->load_from_toml(toml_table);
        if (load_result.is_error()) {
            return {nullptr, load_result.error_code, load_result.error_message};
        }
        
        std::cout << "[DEBUG] Configuration loaded from string" << std::endl;
        return {config, ErrorCode::SUCCESS, ""};
        
    } catch (const std::exception& e) {
        return {nullptr, ErrorCode::INTERNAL_ERROR, 
               std::string("Failed to load configuration from string: ") + e.what()};
    }
}

void SystemConfig::enable_hot_reload(uint32_t check_interval_ms,
                                    std::function<void(const SystemConfig&)> callback) {
    if (config_file_path_.empty()) {
        std::cout << "[WARN] Cannot enable hot reload: no config file path set" << std::endl;
        return;
    }
    
    std::lock_guard<std::shared_mutex> lock(config_mutex_);
    
    if (hot_reload_enabled_.load()) {
        std::cout << "[WARN] Hot reload already enabled" << std::endl;
        return;
    }
    
    hot_reload_interval_ms_ = check_interval_ms;
    hot_reload_callback_ = std::move(callback);
    hot_reload_stop_flag_.store(false);
    hot_reload_enabled_.store(true);
    
    hot_reload_thread_ = std::make_unique<std::thread>(&SystemConfig::hot_reload_worker, this);
    
    LOG_INFO("Hot reload enabled with {}ms interval", check_interval_ms);
}

void SystemConfig::disable_hot_reload() {
    if (!hot_reload_enabled_.load()) {
        return;
    }
    
    hot_reload_stop_flag_.store(true);
    hot_reload_enabled_.store(false);
    
    if (hot_reload_thread_ && hot_reload_thread_->joinable()) {
        hot_reload_thread_->join();
        hot_reload_thread_.reset();
    }
    
    LOG_INFO("Hot reload disabled");
}

Result<void> SystemConfig::reload() {
    if (config_file_path_.empty()) {
        return {ErrorCode::INVALID_REQUEST, "No config file path set for reload"};
    }
    
    try {
        if (!std::filesystem::exists(config_file_path_)) {
            return {ErrorCode::INVALID_REQUEST, 
                   "Configuration file does not exist: " + config_file_path_};
        }
        
        // Parse TOML file
        toml::table toml_table;
        try {
            toml_table = toml::parse_file(config_file_path_);
        } catch (const toml::parse_error& e) {
            return {ErrorCode::INVALID_JSON_FORMAT, 
                   std::string("TOML parsing failed: ") + e.what()};
        }
        
        std::unique_lock<std::shared_mutex> lock(config_mutex_);
        
        auto load_result = load_from_toml(toml_table);
        if (load_result.is_error()) {
            return load_result;
        }
        
        // Update file modification time
        auto file_time = std::filesystem::last_write_time(config_file_path_);
        last_modified_ = std::chrono::system_clock::from_time_t(
            std::chrono::duration_cast<std::chrono::seconds>(
                file_time.time_since_epoch()).count());
        
        LOG_INFO("Configuration reloaded successfully");
        return {ErrorCode::SUCCESS, ""};
        
    } catch (const std::exception& e) {
        return {ErrorCode::INTERNAL_ERROR, 
               std::string("Failed to reload configuration: ") + e.what()};
    }
}

ServerConfig SystemConfig::get_server_config() const {
    std::shared_lock<std::shared_mutex> lock(config_mutex_);
    return server_config_;
}

FeatureConfig SystemConfig::get_feature_config() const {
    std::shared_lock<std::shared_mutex> lock(config_mutex_);
    return feature_config_;
}

LoggingConfig SystemConfig::get_logging_config() const {
    std::shared_lock<std::shared_mutex> lock(config_mutex_);
    return logging_config_;
}

MonitoringConfig SystemConfig::get_monitoring_config() const {
    std::shared_lock<std::shared_mutex> lock(config_mutex_);
    return monitoring_config_;
}

bool SystemConfig::is_valid() const {
    std::shared_lock<std::shared_mutex> lock(config_mutex_);
    return server_config_.is_valid() &&
           feature_config_.is_valid() &&
           logging_config_.is_valid() &&
           monitoring_config_.is_valid();
}

std::string SystemConfig::get_config_path() const {
    std::shared_lock<std::shared_mutex> lock(config_mutex_);
    return config_file_path_;
}

std::chrono::system_clock::time_point SystemConfig::get_last_modified() const {
    std::shared_lock<std::shared_mutex> lock(config_mutex_);
    return last_modified_;
}

Result<void> SystemConfig::load_from_toml(const toml::table& table) {
    // Load server configuration
    auto server_result = ServerConfig::from_toml(table);
    if (server_result.is_error()) {
        return {server_result.error_code, "Server config: " + server_result.error_message};
    }
    server_config_ = server_result.value;
    
    // Load feature configuration
    auto feature_result = FeatureConfig::from_toml(table);
    if (feature_result.is_error()) {
        return {feature_result.error_code, "Feature config: " + feature_result.error_message};
    }
    feature_config_ = feature_result.value;
    
    // Load logging configuration
    auto logging_result = LoggingConfig::from_toml(table);
    if (logging_result.is_error()) {
        return {logging_result.error_code, "Logging config: " + logging_result.error_message};
    }
    logging_config_ = logging_result.value;
    
    // Load monitoring configuration
    auto monitoring_result = MonitoringConfig::from_toml(table);
    if (monitoring_result.is_error()) {
        return {monitoring_result.error_code, "Monitoring config: " + monitoring_result.error_message};
    }
    monitoring_config_ = monitoring_result.value;
    
    return {ErrorCode::SUCCESS, ""};
}

bool SystemConfig::is_file_modified() const {
    if (config_file_path_.empty()) {
        return false;
    }
    
    try {
        if (!std::filesystem::exists(config_file_path_)) {
            return false;
        }
        
        auto file_time = std::filesystem::last_write_time(config_file_path_);
        auto current_time = std::chrono::system_clock::from_time_t(
            std::chrono::duration_cast<std::chrono::seconds>(
                file_time.time_since_epoch()).count());
        
        return current_time > last_modified_;
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Error checking file modification time: " << e.what() << std::endl;
        return false;
    }
}

void SystemConfig::hot_reload_worker() {
    std::cout << "[DEBUG] Hot reload worker started" << std::endl;
    
    while (!hot_reload_stop_flag_.load()) {
        try {
            std::this_thread::sleep_for(std::chrono::milliseconds(hot_reload_interval_ms_));
            
            if (hot_reload_stop_flag_.load()) {
                break;
            }
            
            if (is_file_modified()) {
                LOG_INFO("Configuration file modified, reloading...");
                
                auto reload_result = reload();
                if (reload_result.is_error()) {
                    std::cerr << "[ERROR] Failed to reload configuration: " << reload_result.error_message << std::endl;
                    continue;
                }
                
                // Call callback if provided
                if (hot_reload_callback_) {
                    try {
                        hot_reload_callback_(*this);
                    } catch (const std::exception& e) {
                        std::cerr << "[ERROR] Error in hot reload callback: " << e.what() << std::endl;
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "[ERROR] Error in hot reload worker: " << e.what() << std::endl;
        }
    }
    
    std::cout << "[DEBUG] Hot reload worker stopped" << std::endl;
}

// Global configuration functions
std::shared_ptr<SystemConfig> get_system_config() {
    std::lock_guard<std::mutex> lock(SystemConfig::instance_mutex_);
    return SystemConfig::instance_;
}

void set_system_config(std::shared_ptr<SystemConfig> config) {
    std::lock_guard<std::mutex> lock(SystemConfig::instance_mutex_);
    SystemConfig::instance_ = std::move(config);
}

} // namespace dmp
