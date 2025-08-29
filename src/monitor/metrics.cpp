#include "utils/metrics.hpp"
#include <thread>
#include <sstream>
#include <iomanip>
#include <iostream>

namespace dmp {

// Static instance
MetricsCollector& MetricsCollector::instance() {
    static MetricsCollector instance;
    return instance;
}

Result<void> MetricsCollector::initialize(uint16_t port, const std::string& path) {
    try {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        
        // Reset metrics
        metrics_ = SimpleMetrics{};
        
        initialized_.store(true);
        
        std::cout << "📊 Simplified metrics system initialized (Phase 1)" << std::endl;
        std::cout << "📊 Metrics endpoint placeholder: http://0.0.0.0:" << port << path << std::endl;
        
        return {ErrorCode::SUCCESS, ""};
        
    } catch (const std::exception& e) {
        return {ErrorCode::INTERNAL_ERROR, 
               std::string("Failed to initialize metrics system: ") + e.what()};
    }
}

void MetricsCollector::shutdown() {
    if (initialized_.load()) {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        
        // Print final metrics summary
        std::cout << "📊 Metrics Summary:" << std::endl;
        std::cout << "   Total HTTP Requests: " << metrics_.http_requests_total << std::endl;
        std::cout << "   Total Decisions: " << metrics_.decisions_total << std::endl;
        std::cout << "   Total Errors: " << metrics_.errors_total << std::endl;
        
        if (metrics_.http_requests_total > 0) {
            double avg_request_time = metrics_.total_request_time_ms / metrics_.http_requests_total;
            std::cout << "   Average Request Time: " << std::fixed << std::setprecision(2) 
                      << avg_request_time << "ms" << std::endl;
        }
        
        if (metrics_.decisions_total > 0) {
            double avg_decision_time = metrics_.total_decision_time_ms / metrics_.decisions_total;
            std::cout << "   Average Decision Time: " << std::fixed << std::setprecision(2) 
                      << avg_decision_time << "ms" << std::endl;
        }
        
        initialized_.store(false);
        std::cout << "📊 Metrics system shutdown completed" << std::endl;
    }
}

void MetricsCollector::record_http_request(const std::string& method, const std::string& path,
                                         int status_code, double duration_ms) {
    if (!initialized_.load()) return;
    
    try {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        
        metrics_.http_requests_total++;
        metrics_.total_request_time_ms += duration_ms;
        
        // Log to console for Phase 1
        std::cout << "📊 HTTP: " << method << " " << path << " -> " << status_code 
                  << " (" << std::fixed << std::setprecision(2) << duration_ms << "ms)" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Failed to record HTTP request metrics: " << e.what() << std::endl;
    }
}

void MetricsCollector::record_decision(Decision decision, float risk_score, 
                                     double processing_time_ms) {
    if (!initialized_.load()) return;
    
    try {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        
        metrics_.decisions_total++;
        metrics_.total_decision_time_ms += processing_time_ms;
        
        // Log to console for Phase 1
        std::cout << "📊 Decision: " << decision_to_string(decision) 
                  << " (score: " << std::fixed << std::setprecision(1) << risk_score 
                  << ", time: " << std::setprecision(2) << processing_time_ms << "ms)" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Failed to record decision metrics: " << e.what() << std::endl;
    }
}

void MetricsCollector::record_rule_evaluation(int rules_evaluated, int rules_triggered,
                                            double evaluation_time_ms) {
    if (!initialized_.load()) return;
    
    try {
        // Log to console for Phase 1
        std::cout << "📊 Rules: evaluated=" << rules_evaluated 
                  << ", triggered=" << rules_triggered
                  << " (" << std::fixed << std::setprecision(2) << evaluation_time_ms << "ms)" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Failed to record rule evaluation metrics: " << e.what() << std::endl;
    }
}

void MetricsCollector::record_feature_extraction(bool cache_hit, double extraction_time_ms,
                                                int feature_count) {
    if (!initialized_.load()) return;
    
    try {
        // Log to console for Phase 1
        std::cout << "📊 Features: " << (cache_hit ? "cache_hit" : "cache_miss") 
                  << ", count=" << feature_count
                  << " (" << std::fixed << std::setprecision(2) << extraction_time_ms << "ms)" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Failed to record feature extraction metrics: " << e.what() << std::endl;
    }
}

void MetricsCollector::record_ml_inference(const std::string& model_name, 
                                         double inference_time_ms, float prediction_score) {
    if (!initialized_.load()) return;
    
    try {
        // Log to console for Phase 1
        std::cout << "📊 ML: model=" << model_name 
                  << ", score=" << std::fixed << std::setprecision(3) << prediction_score
                  << " (" << std::setprecision(2) << inference_time_ms << "ms)" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Failed to record ML inference metrics: " << e.what() << std::endl;
    }
}

void MetricsCollector::update_system_metrics(double cpu_usage_percent, 
                                            double memory_usage_mb, int active_connections) {
    if (!initialized_.load()) return;
    
    try {
        // Log to console for Phase 1
        std::cout << "📊 System: CPU=" << std::fixed << std::setprecision(1) << cpu_usage_percent << "%, "
                  << "Memory=" << std::setprecision(1) << memory_usage_mb << "MB, "
                  << "Connections=" << active_connections << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Failed to update system metrics: " << e.what() << std::endl;
    }
}

void MetricsCollector::record_error(const std::string& error_type, 
                                  const std::string& component) {
    if (!initialized_.load()) return;
    
    try {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        
        metrics_.errors_total++;
        
        // Log to console for Phase 1
        std::cout << "📊 Error: " << error_type << " in " << component << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Failed to record error metrics: " << e.what() << std::endl;
    }
}

std::string MetricsCollector::decision_to_string(Decision decision) {
    switch (decision) {
        case Decision::APPROVE: return "APPROVE";
        case Decision::DECLINE: return "DECLINE";
        case Decision::REVIEW: return "REVIEW";
        default: return "UNKNOWN";
    }
}

// MetricsTimer implementation
MetricsTimer::MetricsTimer(const std::string& operation_name)
    : operation_name_(operation_name)
    , start_time_(std::chrono::high_resolution_clock::now())
    , stopped_(false) {
}

MetricsTimer::~MetricsTimer() {
    if (!stopped_) {
        double duration = elapsed_ms();
        std::cout << "📊 Timer: " << operation_name_ << " completed in " 
                  << std::fixed << std::setprecision(2) << duration << "ms" << std::endl;
    }
}

double MetricsTimer::elapsed_ms() const {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - start_time_);
    return duration.count() / 1000.0;
}

// Helper functions
uint64_t get_current_timestamp_ms() {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
}

std::string format_duration(double duration_ms) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1);
    
    if (duration_ms < 1.0) {
        oss << (duration_ms * 1000.0) << "μs";
    } else if (duration_ms < 1000.0) {
        oss << duration_ms << "ms";
    } else {
        oss << (duration_ms / 1000.0) << "s";
    }
    
    return oss.str();
}

} // namespace dmp