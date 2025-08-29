/**
 * @file transaction.hpp
 * @brief Core transaction data structures for DMP risk control system
 * @author Stan Jiang
 * @date 2025-08-28
 */
#pragma once

#include "common/types.hpp"
#include <simdjson.h>
#include <string>
#include <sstream>
#include <optional>

namespace dmp {

/**
 * @brief Transaction information containing payment details
 * 
 * Optimized for high-frequency parsing and validation.
 * All fields are validated during JSON deserialization.
 */
struct TransactionInfo {
    Amount amount;
    std::string currency;
    MerchantId merchant_id;
    uint16_t merchant_category;
    std::string pos_entry_mode;
    
    /**
     * @brief Parse transaction info from JSON element
     * @param json JSON element containing transaction data
     * @return Result with transaction info or error
     */
    static Result<TransactionInfo> from_json(const simdjson::dom::element& json);
    
    /**
     * @brief Convert to JSON string
     * @return JSON representation
     */
    std::string to_json() const;
    
    /**
     * @brief Validate transaction data
     * @return true if valid, false otherwise
     */
    bool is_valid() const;
};

/**
 * @brief Card information for payment instrument validation
 */
struct CardInfo {
    std::string token;
    std::string issuer_country;
    std::string card_brand;
    
    static Result<CardInfo> from_json(const simdjson::dom::element& json);
    std::string to_json() const;
    bool is_valid() const;
};

/**
 * @brief Device fingerprinting information for fraud detection
 */
struct DeviceInfo {
    std::string ip;
    std::string fingerprint;
    std::string user_agent;
    
    static Result<DeviceInfo> from_json(const simdjson::dom::element& json);
    std::string to_json() const;
    bool is_valid() const;
};

/**
 * @brief Customer profile information for risk assessment
 */
struct CustomerInfo {
    UserId id;
    RiskScore risk_score;
    uint32_t account_age_days;
    
    static Result<CustomerInfo> from_json(const simdjson::dom::element& json);
    std::string to_json() const;
    bool is_valid() const;
};

/**
 * @brief Complete transaction request with all context data
 * 
 * Primary input structure for risk control decisions.
 * Optimized for sub-millisecond parsing using simdjson.
 */
struct TransactionRequest {
    RequestId request_id;
    Timestamp timestamp;
    TransactionInfo transaction;
    CardInfo card;
    DeviceInfo device;
    CustomerInfo customer;
    
    /**
     * @brief Parse complete transaction request from JSON
     * @param json JSON element with full request data
     * @return Result containing parsed request or error details
     * 
     * Performance: Target < 0.5ms for typical 2KB request
     * Thread-safe: Yes, uses simdjson DOM parser
     */
    static Result<TransactionRequest> from_json(const simdjson::dom::element& json);
    
    /**
     * @brief Convert to JSON string representation
     * @return Complete JSON representation of the request
     */
    std::string to_json() const;
    
    /**
     * @brief Validate all components of the transaction request
     * @return true if all fields are valid, false otherwise
     */
    bool is_valid() const;
    
    /**
     * @brief Generate cache key for feature lookup
     * @return Unique key for caching extracted features
     * 
     * Uses combination of customer_id, merchant_id, and timestamp window
     * to enable efficient feature cache lookup and storage.
     */
    std::string get_cache_key() const;
};

/**
 * @brief Transaction decision response with detailed reasoning
 * 
 * Contains final decision, risk score, and audit trail for
 * regulatory compliance and debugging.
 */
struct TransactionResponse {
    RequestId request_id;
    Decision decision;
    RiskScore risk_score;
    std::vector<std::string> triggered_rules;
    float latency_ms;
    std::string model_version;
    Timestamp timestamp;
    
    /**
     * @brief Serialize response to JSON format
     * @return JSON string ready for HTTP response
     * 
     * Performance: Target < 0.1ms for serialization
     */
    std::string to_json() const;
    
    /**
     * @brief Validate response completeness
     * @return true if response is complete and valid
     */
    bool is_valid() const;
};

/**
 * @brief Internal decision processing context
 * 
 * Contains intermediate computation results used during
 * the decision pipeline. Not exposed in external APIs.
 */
struct DecisionContext {
    TransactionRequest request;
    FixedFeatureVector features;
    std::vector<float> rule_scores;
    std::vector<float> model_scores;
    
    /**
     * @brief Calculate weighted final risk score
     * @return Combined risk score from all sources
     * 
     * Implements weighted voting algorithm with rule priorities
     * and model confidence scores.
     */
    RiskScore calculate_final_score() const;
    
    /**
     * @brief Generate human-readable decision reasons
     * @return List of triggered rules and their contributions
     * 
     * Used for audit logs and customer explanations.
     */
    std::vector<std::string> generate_reasons() const;
    
    /**
     * @brief Validate decision context completeness
     * @return true if context is ready for decision fusion
     */
    bool is_complete() const;
};

/**
 * @brief Feature vector with metadata for ML inference
 * 
 * Extended feature structure with caching information
 * and version tracking for model compatibility.
 */
struct FeatureSet {
    FixedFeatureVector values;
    uint64_t computed_at;  // Unix timestamp in milliseconds
    uint32_t version;      // Feature schema version
    std::string cache_key; // For cache storage and retrieval
    
    /**
     * @brief Check if features are fresh enough for use
     * @param max_age_ms Maximum age in milliseconds
     * @return true if features are still valid
     */
    bool is_fresh(uint64_t max_age_ms = 300000) const; // Default 5 minutes
    
    /**
     * @brief Serialize for cache storage
     * @return Binary representation for efficient storage
     */
    std::vector<uint8_t> serialize() const;
    
    /**
     * @brief Deserialize from cache storage
     * @param data Binary data from cache
     * @return Parsed feature set or error
     */
    static Result<FeatureSet> deserialize(const std::vector<uint8_t>& data);
};

} // namespace dmp
