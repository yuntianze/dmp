#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <chrono>
#include <optional>

namespace dmp {

// 基础类型定义
using RequestId = std::string;
using Timestamp = std::chrono::time_point<std::chrono::system_clock>;
using UserId = std::string;
using MerchantId = std::string;
using Amount = double;
using RiskScore = float;

// 决策枚举
enum class Decision : uint8_t {
    APPROVE = 0,
    DECLINE = 1,
    REVIEW = 2
};

// 特征向量类型
using FeatureVector = std::vector<float>;
static constexpr size_t FEATURE_VECTOR_SIZE = 64;
using FixedFeatureVector = std::array<float, FEATURE_VECTOR_SIZE>;

// 性能指标类型
struct LatencyMetrics {
    float p50_ms;
    float p95_ms;
    float p99_ms;
    float avg_ms;
};

struct ThroughputMetrics {
    uint64_t requests_per_second;
    uint64_t total_requests;
    uint64_t failed_requests;
};

// 错误码定义
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

// 结果状态
template<typename T>
struct Result {
    T value;
    ErrorCode error_code;
    std::string error_message;
    
    bool is_success() const { return error_code == ErrorCode::SUCCESS; }
    bool is_error() const { return error_code != ErrorCode::SUCCESS; }
};

} // namespace dmp
