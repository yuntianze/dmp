/**
 * @file types.hpp
 * @brief Basic type definitions for DMP risk control system
 * @author Stan Jiang
 * @date 2025-08-28
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <chrono>
#include <optional>

namespace dmp {

/**
 * @brief Basic type definitions for DMP risk control system
 */
using RequestId = std::string;
using Timestamp = std::chrono::time_point<std::chrono::system_clock>;
using UserId = std::string;
using MerchantId = std::string;
using Amount = double;
using RiskScore = float;

/**
 * @brief Decision outcome enumeration for risk control
 */
enum class Decision : uint8_t {
    APPROVE = 0,  // Transaction approved
    DECLINE = 1,  // Transaction declined
    REVIEW = 2    // Transaction requires manual review
};

/**
 * @brief Feature vector type definitions
 */
using FeatureVector = std::vector<float>;
static constexpr size_t FEATURE_VECTOR_SIZE = 64;
using FixedFeatureVector = std::array<float, FEATURE_VECTOR_SIZE>;

/**
 * @brief Performance metrics structures
 */
struct LatencyMetrics {
    float p50_ms;      // 50th percentile latency
    float p95_ms;      // 95th percentile latency
    float p99_ms;      // 99th percentile latency
    float avg_ms;      // Average latency
};

struct ThroughputMetrics {
    uint64_t requests_per_second;  // Current RPS
    uint64_t total_requests;       // Total requests processed
    uint64_t failed_requests;      // Total failed requests
};

/**
 * @brief Error codes for DMP system operations
 */
enum class ErrorCode : uint32_t {
    SUCCESS = 0,
    INVALID_REQUEST = 1001,
    MISSING_REQUIRED_FIELD = 1002,
    INVALID_JSON_FORMAT = 1003,
    FEATURE_EXTRACTION_FAILED = 2001,
    RULE_EVALUATION_FAILED = 2002,
    MODEL_INFERENCE_FAILED = 2003,
    CACHE_ERROR = 3001,
    DATABASE_ERROR = 3002,
    INTERNAL_ERROR = 9999
};

/**
 * @brief Result type for operations that may fail
 *
 * Provides a standardized way to handle operation results with error information.
 */
template<typename T>
struct Result {
    T value;                    // Operation result value
    ErrorCode error_code;       // Error code if operation failed
    std::string error_message;  // Human-readable error message

    bool is_success() const { return error_code == ErrorCode::SUCCESS; }
    bool is_error() const { return error_code != ErrorCode::SUCCESS; }
};

// Result<void> 特化
template<>
struct Result<void> {
    ErrorCode error_code;
    std::string error_message;
    
    Result() : error_code(ErrorCode::SUCCESS) {}
    Result(ErrorCode code, const std::string& message = "") 
        : error_code(code), error_message(message) {}
    
    bool is_success() const { return error_code == ErrorCode::SUCCESS; }
    bool is_error() const { return error_code != ErrorCode::SUCCESS; }
};

} // namespace dmp
