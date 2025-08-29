/**
 * @file metrics.hpp
 * @brief Simplified metrics collection system for DMP risk control (Phase 1)
 * @author Stan Jiang
 * @date 2025-08-28
 */
#pragma once

#include "common/types.hpp"
#include <memory>
#include <string>
#include <atomic>
#include <chrono>
#include <map>
#include <mutex>

namespace dmp {

/**
 * @brief Simplified metrics collection system for Phase 1
 * 
 * Provides basic metric collection with console output.
 * Thread-safe operations for concurrent access from multiple request handlers.
 */
class MetricsCollector {
public:
    /**
     * @brief Get singleton instance of metrics collector
     * @return Reference to global metrics collector
     */
    static MetricsCollector& instance();
    
    /**
     * @brief Initialize metrics system
     * @param port Port for metrics endpoint (placeholder for Phase 1)
     * @param path Path for metrics endpoint (placeholder for Phase 1)
     * @return Success or failure
     */
    Result<void> initialize(uint16_t port = 9090, const std::string& path = "/metrics");
    
    /**
     * @brief Shutdown metrics system
     */
    void shutdown();
    
    /**
     * @brief Record HTTP request metrics
     * @param method HTTP method (GET, POST, etc.)
     * @param path Request path
     * @param status_code HTTP status code
     * @param duration_ms Request duration in milliseconds
     */
    void record_http_request(const std::string& method, const std::string& path,
                           int status_code, double duration_ms);
    
    /**
     * @brief Record decision metrics
     * @param decision Decision result (APPROVE, DECLINE, REVIEW)
     * @param risk_score Risk score value
     * @param processing_time_ms Processing time in milliseconds
     */
    void record_decision(Decision decision, float risk_score, double processing_time_ms);
    
    /**
     * @brief Record rule engine metrics
     * @param rules_evaluated Number of rules evaluated
     * @param rules_triggered Number of rules triggered
     * @param evaluation_time_ms Rule evaluation time
     */
    void record_rule_evaluation(int rules_evaluated, int rules_triggered, 
                               double evaluation_time_ms);
    
    /**
     * @brief Record feature extraction metrics
     * @param cache_hit Whether feature was found in cache
     * @param extraction_time_ms Feature extraction time
     * @param feature_count Number of features extracted
     */
    void record_feature_extraction(bool cache_hit, double extraction_time_ms, 
                                  int feature_count);
    
    /**
     * @brief Record ML inference metrics
     * @param model_name Name of the model used
     * @param inference_time_ms Inference time in milliseconds
     * @param prediction_score Model prediction score
     */
    void record_ml_inference(const std::string& model_name, double inference_time_ms,
                           float prediction_score);
    
    /**
     * @brief Update system resource metrics
     * @param cpu_usage_percent CPU usage percentage
     * @param memory_usage_mb Memory usage in MB
     * @param active_connections Number of active connections
     */
    void update_system_metrics(double cpu_usage_percent, double memory_usage_mb,
                             int active_connections);
    
    /**
     * @brief Record error occurrence
     * @param error_type Type of error
     * @param component Component where error occurred
     */
    void record_error(const std::string& error_type, const std::string& component);
    
    /**
     * @brief Check if metrics system is initialized
     * @return true if initialized and ready
     */
    bool is_initialized() const { return initialized_.load(); }

private:
    MetricsCollector() = default;
    ~MetricsCollector() = default;
    
    // Delete copy constructor and assignment operator
    MetricsCollector(const MetricsCollector&) = delete;
    MetricsCollector& operator=(const MetricsCollector&) = delete;
    
    /**
     * @brief Convert decision enum to string
     */
    std::string decision_to_string(Decision decision);

    // Simplified metrics storage
    std::atomic<bool> initialized_{false};
    std::mutex metrics_mutex_;
    
    // Basic counters for Phase 1
    struct SimpleMetrics {
        uint64_t http_requests_total = 0;
        uint64_t decisions_total = 0;
        uint64_t errors_total = 0;
        double total_request_time_ms = 0.0;
        double total_decision_time_ms = 0.0;
    } metrics_;
};

/**
 * @brief RAII helper for measuring operation duration
 * 
 * Automatically records operation duration when scope exits.
 * Use this for measuring function execution times.
 */
class MetricsTimer {
public:
    /**
     * @brief Constructor - starts timing
     * @param operation_name Name of the operation being timed
     */
    explicit MetricsTimer(const std::string& operation_name);
    
    /**
     * @brief Destructor - records elapsed time
     */
    ~MetricsTimer();
    
    /**
     * @brief Get elapsed time so far
     * @return Elapsed time in milliseconds
     */
    double elapsed_ms() const;

private:
    std::string operation_name_;
    std::chrono::high_resolution_clock::time_point start_time_;
    bool stopped_;
};

/**
 * @brief Macro for easy timing of code blocks
 * 
 * Usage:
 * {
 *     DMP_TIME_OPERATION("decision_processing");
 *     // ... code to time ...
 * } // Automatically records timing when scope exits
 */
#define DMP_TIME_OPERATION(name) \
    MetricsTimer _timer_##__LINE__(name)

/**
 * @brief Helper function to get current timestamp in milliseconds
 * @return Current timestamp as milliseconds since epoch
 */
uint64_t get_current_timestamp_ms();

/**
 * @brief Helper function to format duration as human-readable string
 * @param duration_ms Duration in milliseconds
 * @return Formatted string (e.g., "12.5ms", "1.2s")
 */
std::string format_duration(double duration_ms);

} // namespace dmp