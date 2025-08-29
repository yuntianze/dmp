#include "core/transaction.hpp"
#include <chrono>
#include <regex>
#include <iomanip>
#include <cstring>
#include <iostream>

namespace dmp {

namespace {
    // JSON field names as constants for consistency
    constexpr const char* kRequestId = "request_id";
    constexpr const char* kTimestamp = "timestamp";
    constexpr const char* kTransaction = "transaction";
    constexpr const char* kCard = "card";
    constexpr const char* kDevice = "device";
    constexpr const char* kCustomer = "customer";
    constexpr const char* kAmount = "amount";
    constexpr const char* kCurrency = "currency";
    constexpr const char* kMerchantId = "merchant_id";
    constexpr const char* kMerchantCategory = "merchant_category";
    constexpr const char* kPosEntryMode = "pos_entry_mode";
    constexpr const char* kToken = "token";
    constexpr const char* kIssuerCountry = "issuer_country";
    constexpr const char* kCardBrand = "card_brand";
    constexpr const char* kIp = "ip";
    constexpr const char* kFingerprint = "fingerprint";
    constexpr const char* kUserAgent = "user_agent";
    constexpr const char* kId = "id";
    constexpr const char* kRiskScore = "risk_score";
    constexpr const char* kAccountAgeDays = "account_age_days";
    
    // Validation constants
    constexpr double kMinAmount = 0.01;
    constexpr double kMaxAmount = 1000000.0; // $1M limit
    constexpr size_t kMaxStringLength = 512;
    
    /**
     * @brief Helper function to safely extract string from JSON
     * @param element JSON element
     * @param max_length Maximum allowed string length
     * @return String value or error
     */
    Result<std::string> extract_string(const simdjson::dom::element& element, 
                                      size_t max_length = kMaxStringLength) {
        try {
            std::string_view value = element.get_string();
            if (value.length() > max_length) {
                return {std::string{}, ErrorCode::INVALID_REQUEST, 
                       "String field exceeds maximum length"};
            }
            return {std::string{value}, ErrorCode::SUCCESS, ""};
        } catch (const simdjson::simdjson_error& e) {
            return {std::string{}, ErrorCode::INVALID_JSON_FORMAT, 
                   std::string("JSON string extraction failed: ") + e.what()};
        }
    }
    
    /**
     * @brief Helper function to safely extract double from JSON
     */
    Result<double> extract_double(const simdjson::dom::element& element) {
        try {
            double value = element.get_double();
            return {value, ErrorCode::SUCCESS, ""};
        } catch (const simdjson::simdjson_error& e) {
            return {0.0, ErrorCode::INVALID_JSON_FORMAT, 
                   std::string("JSON double extraction failed: ") + e.what()};
        }
    }
    
    /**
     * @brief Helper function to safely extract uint64 from JSON
     */
    Result<uint64_t> extract_uint64(const simdjson::dom::element& element) {
        try {
            uint64_t value = element.get_uint64();
            return {value, ErrorCode::SUCCESS, ""};
        } catch (const simdjson::simdjson_error& e) {
            return {0, ErrorCode::INVALID_JSON_FORMAT, 
                   std::string("JSON uint64 extraction failed: ") + e.what()};
        }
    }
    
    /**
     * @brief Helper function to safely extract uint32 from JSON
     */
    Result<uint32_t> extract_uint32(const simdjson::dom::element& element) {
        try {
            uint64_t temp = element.get_uint64();
            if (temp > std::numeric_limits<uint32_t>::max()) {
                return {0, ErrorCode::INVALID_REQUEST, "Value exceeds uint32 range"};
            }
            return {static_cast<uint32_t>(temp), ErrorCode::SUCCESS, ""};
        } catch (const simdjson::simdjson_error& e) {
            return {0, ErrorCode::INVALID_JSON_FORMAT, 
                   std::string("JSON uint32 extraction failed: ") + e.what()};
        }
    }
    
    /**
     * @brief Convert Decision enum to string
     */
    std::string decision_to_string(Decision decision) {
        switch (decision) {
            case Decision::APPROVE: return "APPROVE";
            case Decision::DECLINE: return "DECLINE";
            case Decision::REVIEW: return "REVIEW";
            default: return "UNKNOWN";
        }
    }
}

// TransactionInfo implementation
Result<TransactionInfo> TransactionInfo::from_json(const simdjson::dom::element& json) {
    TransactionInfo info;
    
    try {
        // Extract amount with validation
        auto amount_result = extract_double(json[kAmount]);
        if (amount_result.is_error()) {
            return {info, amount_result.error_code, amount_result.error_message};
        }
        info.amount = amount_result.value;
        
        // Validate amount range
        if (info.amount < kMinAmount || info.amount > kMaxAmount) {
            return {info, ErrorCode::INVALID_REQUEST, 
                   "Transaction amount out of valid range"};
        }
        
        // Extract currency
        auto currency_result = extract_string(json[kCurrency], 3);
        if (currency_result.is_error()) {
            return {info, currency_result.error_code, currency_result.error_message};
        }
        info.currency = currency_result.value;
        
        // Extract merchant ID
        auto merchant_result = extract_string(json[kMerchantId], 50);
        if (merchant_result.is_error()) {
            return {info, merchant_result.error_code, merchant_result.error_message};
        }
        info.merchant_id = merchant_result.value;
        
        // Extract merchant category
        auto category_result = extract_uint32(json[kMerchantCategory]);
        if (category_result.is_error()) {
            return {info, category_result.error_code, category_result.error_message};
        }
        info.merchant_category = static_cast<uint16_t>(category_result.value);
        
        // Extract POS entry mode
        auto pos_result = extract_string(json[kPosEntryMode], 20);
        if (pos_result.is_error()) {
            return {info, pos_result.error_code, pos_result.error_message};
        }
        info.pos_entry_mode = pos_result.value;
        
    } catch (const simdjson::simdjson_error& e) {
        return {info, ErrorCode::INVALID_JSON_FORMAT, 
               std::string("TransactionInfo JSON parsing failed: ") + e.what()};
    }
    
    return {info, ErrorCode::SUCCESS, ""};
}

std::string TransactionInfo::to_json() const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    oss << "{"
        << "\"" << kAmount << "\":" << amount << ","
        << "\"" << kCurrency << "\":\"" << currency << "\","
        << "\"" << kMerchantId << "\":\"" << merchant_id << "\","
        << "\"" << kMerchantCategory << "\":" << merchant_category << ","
        << "\"" << kPosEntryMode << "\":\"" << pos_entry_mode << "\""
        << "}";
    return oss.str();
}

bool TransactionInfo::is_valid() const {
    return amount >= kMinAmount && amount <= kMaxAmount &&
           !currency.empty() && currency.length() <= 3 &&
           !merchant_id.empty() && merchant_id.length() <= 50 &&
           merchant_category > 0 &&
           !pos_entry_mode.empty() && pos_entry_mode.length() <= 20;
}

// CardInfo implementation
Result<CardInfo> CardInfo::from_json(const simdjson::dom::element& json) {
    CardInfo info;
    
    try {
        auto token_result = extract_string(json[kToken], 100);
        if (token_result.is_error()) {
            return {info, token_result.error_code, token_result.error_message};
        }
        info.token = token_result.value;
        
        auto country_result = extract_string(json[kIssuerCountry], 2);
        if (country_result.is_error()) {
            return {info, country_result.error_code, country_result.error_message};
        }
        info.issuer_country = country_result.value;
        
        auto brand_result = extract_string(json[kCardBrand], 20);
        if (brand_result.is_error()) {
            return {info, brand_result.error_code, brand_result.error_message};
        }
        info.card_brand = brand_result.value;
        
    } catch (const simdjson::simdjson_error& e) {
        return {info, ErrorCode::INVALID_JSON_FORMAT, 
               std::string("CardInfo JSON parsing failed: ") + e.what()};
    }
    
    return {info, ErrorCode::SUCCESS, ""};
}

std::string CardInfo::to_json() const {
    std::ostringstream oss;
    oss << "{"
        << "\"" << kToken << "\":\"" << token << "\","
        << "\"" << kIssuerCountry << "\":\"" << issuer_country << "\","
        << "\"" << kCardBrand << "\":\"" << card_brand << "\""
        << "}";
    return oss.str();
}

bool CardInfo::is_valid() const {
    return !token.empty() && token.length() <= 100 &&
           !issuer_country.empty() && issuer_country.length() <= 2 &&
           !card_brand.empty() && card_brand.length() <= 20;
}

// DeviceInfo implementation
Result<DeviceInfo> DeviceInfo::from_json(const simdjson::dom::element& json) {
    DeviceInfo info;
    
    try {
        auto ip_result = extract_string(json[kIp], 45); // IPv6 max length
        if (ip_result.is_error()) {
            return {info, ip_result.error_code, ip_result.error_message};
        }
        info.ip = ip_result.value;
        
        auto fingerprint_result = extract_string(json[kFingerprint], 100);
        if (fingerprint_result.is_error()) {
            return {info, fingerprint_result.error_code, fingerprint_result.error_message};
        }
        info.fingerprint = fingerprint_result.value;
        
        auto ua_result = extract_string(json[kUserAgent], 500);
        if (ua_result.is_error()) {
            return {info, ua_result.error_code, ua_result.error_message};
        }
        info.user_agent = ua_result.value;
        
    } catch (const simdjson::simdjson_error& e) {
        return {info, ErrorCode::INVALID_JSON_FORMAT, 
               std::string("DeviceInfo JSON parsing failed: ") + e.what()};
    }
    
    return {info, ErrorCode::SUCCESS, ""};
}

std::string DeviceInfo::to_json() const {
    std::ostringstream oss;
    oss << "{"
        << "\"" << kIp << "\":\"" << ip << "\","
        << "\"" << kFingerprint << "\":\"" << fingerprint << "\","
        << "\"" << kUserAgent << "\":\"" << user_agent << "\""
        << "}";
    return oss.str();
}

bool DeviceInfo::is_valid() const {
    // Basic IP validation using regex
    static const std::regex ipv4_regex(
        R"(^(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$)");
    static const std::regex ipv6_regex(
        R"(^(?:[0-9a-fA-F]{1,4}:){7}[0-9a-fA-F]{1,4}$|^::1$|^::$)");
    
    bool valid_ip = std::regex_match(ip, ipv4_regex) || std::regex_match(ip, ipv6_regex);
    
    return valid_ip &&
           !fingerprint.empty() && fingerprint.length() <= 100 &&
           !user_agent.empty() && user_agent.length() <= 500;
}

// CustomerInfo implementation
Result<CustomerInfo> CustomerInfo::from_json(const simdjson::dom::element& json) {
    CustomerInfo info;
    
    try {
        auto id_result = extract_string(json[kId], 50);
        if (id_result.is_error()) {
            return {info, id_result.error_code, id_result.error_message};
        }
        info.id = id_result.value;
        
        auto risk_result = extract_double(json[kRiskScore]);
        if (risk_result.is_error()) {
            return {info, risk_result.error_code, risk_result.error_message};
        }
        info.risk_score = static_cast<RiskScore>(risk_result.value);
        
        auto age_result = extract_uint32(json[kAccountAgeDays]);
        if (age_result.is_error()) {
            return {info, age_result.error_code, age_result.error_message};
        }
        info.account_age_days = age_result.value;
        
    } catch (const simdjson::simdjson_error& e) {
        return {info, ErrorCode::INVALID_JSON_FORMAT, 
               std::string("CustomerInfo JSON parsing failed: ") + e.what()};
    }
    
    return {info, ErrorCode::SUCCESS, ""};
}

std::string CustomerInfo::to_json() const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    oss << "{"
        << "\"" << kId << "\":\"" << id << "\","
        << "\"" << kRiskScore << "\":" << risk_score << ","
        << "\"" << kAccountAgeDays << "\":" << account_age_days
        << "}";
    return oss.str();
}

bool CustomerInfo::is_valid() const {
    return !id.empty() && id.length() <= 50 &&
           risk_score >= 0.0f && risk_score <= 100.0f &&
           account_age_days <= 36500; // ~100 years max
}

// TransactionRequest implementation
Result<TransactionRequest> TransactionRequest::from_json(const simdjson::dom::element& json) {
    TransactionRequest request;
    
    try {
        // Extract request ID
        auto request_id_result = extract_string(json[kRequestId], 100);
        if (request_id_result.is_error()) {
            return {request, request_id_result.error_code, request_id_result.error_message};
        }
        request.request_id = request_id_result.value;
        
        // Extract timestamp
        auto timestamp_result = extract_uint64(json[kTimestamp]);
        if (timestamp_result.is_error()) {
            return {request, timestamp_result.error_code, timestamp_result.error_message};
        }
        request.timestamp = Timestamp(std::chrono::milliseconds(timestamp_result.value));
        
        // Parse nested transaction info
        auto transaction_result = TransactionInfo::from_json(json[kTransaction]);
        if (transaction_result.is_error()) {
            return {request, transaction_result.error_code, 
                   "Transaction info: " + transaction_result.error_message};
        }
        request.transaction = transaction_result.value;
        
        // Parse nested card info
        auto card_result = CardInfo::from_json(json[kCard]);
        if (card_result.is_error()) {
            return {request, card_result.error_code, 
                   "Card info: " + card_result.error_message};
        }
        request.card = card_result.value;
        
        // Parse nested device info
        auto device_result = DeviceInfo::from_json(json[kDevice]);
        if (device_result.is_error()) {
            return {request, device_result.error_code, 
                   "Device info: " + device_result.error_message};
        }
        request.device = device_result.value;
        
        // Parse nested customer info
        auto customer_result = CustomerInfo::from_json(json[kCustomer]);
        if (customer_result.is_error()) {
            return {request, customer_result.error_code, 
                   "Customer info: " + customer_result.error_message};
        }
        request.customer = customer_result.value;
        
    } catch (const simdjson::simdjson_error& e) {
        return {request, ErrorCode::INVALID_JSON_FORMAT, 
               std::string("TransactionRequest JSON parsing failed: ") + e.what()};
    }
    
    return {request, ErrorCode::SUCCESS, ""};
}

std::string TransactionRequest::to_json() const {
    std::ostringstream oss;
    auto timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        timestamp.time_since_epoch()).count();
    
    oss << "{"
        << "\"" << kRequestId << "\":\"" << request_id << "\","
        << "\"" << kTimestamp << "\":" << timestamp_ms << ","
        << "\"" << kTransaction << "\":" << transaction.to_json() << ","
        << "\"" << kCard << "\":" << card.to_json() << ","
        << "\"" << kDevice << "\":" << device.to_json() << ","
        << "\"" << kCustomer << "\":" << customer.to_json()
        << "}";
    return oss.str();
}

bool TransactionRequest::is_valid() const {
    // Check timestamp is not too far in future (1 hour max)
    auto now = std::chrono::system_clock::now();
    auto max_future = now + std::chrono::hours(1);
    
    return !request_id.empty() && request_id.length() <= 100 &&
           timestamp <= max_future &&
           transaction.is_valid() &&
           card.is_valid() &&
           device.is_valid() &&
           customer.is_valid();
}

std::string TransactionRequest::get_cache_key() const {
    // Generate cache key using customer ID, merchant ID, and 5-minute time window
    auto timestamp_5min = std::chrono::duration_cast<std::chrono::minutes>(
        timestamp.time_since_epoch()).count() / 5;
    
    std::ostringstream oss;
    oss << "features:" << customer.id << ":" << transaction.merchant_id 
        << ":" << timestamp_5min;
    return oss.str();
}

// TransactionResponse implementation
std::string TransactionResponse::to_json() const {
    std::ostringstream oss;
    auto timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        timestamp.time_since_epoch()).count();
    
    oss << std::fixed << std::setprecision(2);
    oss << "{"
        << "\"" << kRequestId << "\":\"" << request_id << "\","
        << "\"decision\":\"" << decision_to_string(decision) << "\","
        << "\"risk_score\":" << risk_score << ","
        << "\"reasons\":[";
    
    // Serialize triggered rules array
    for (size_t i = 0; i < triggered_rules.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "\"" << triggered_rules[i] << "\"";
    }
    
    oss << "],"
        << "\"latency_ms\":" << latency_ms << ","
        << "\"model_version\":\"" << model_version << "\","
        << "\"timestamp\":" << timestamp_ms
        << "}";
    return oss.str();
}

bool TransactionResponse::is_valid() const {
    return !request_id.empty() &&
           risk_score >= 0.0f && risk_score <= 100.0f &&
           latency_ms >= 0.0f &&
           !model_version.empty();
}

// DecisionContext implementation
RiskScore DecisionContext::calculate_final_score() const {
    if (rule_scores.empty() && model_scores.empty()) {
        return 0.0f; // Default neutral score
    }
    
    // Weighted voting: rules have 60% weight, models have 40%
    constexpr float kRuleWeight = 0.6f;
    constexpr float kModelWeight = 0.4f;
    
    float rule_contribution = 0.0f;
    if (!rule_scores.empty()) {
        float sum = 0.0f;
        for (float score : rule_scores) {
            sum += score;
        }
        rule_contribution = (sum / rule_scores.size()) * kRuleWeight;
    }
    
    float model_contribution = 0.0f;
    if (!model_scores.empty()) {
        float sum = 0.0f;
        for (float score : model_scores) {
            sum += score;
        }
        model_contribution = (sum / model_scores.size()) * kModelWeight;
    }
    
    return std::clamp(rule_contribution + model_contribution, 0.0f, 100.0f);
}

std::vector<std::string> DecisionContext::generate_reasons() const {
    std::vector<std::string> reasons;
    
    // Add rule-based reasons
    for (size_t i = 0; i < rule_scores.size(); ++i) {
        if (rule_scores[i] > 50.0f) { // High risk threshold
            reasons.push_back("Rule " + std::to_string(i + 1) + " triggered (score: " + 
                            std::to_string(static_cast<int>(rule_scores[i])) + ")");
        }
    }
    
    // Add model-based reasons
    if (!model_scores.empty()) {
        float max_model_score = *std::max_element(model_scores.begin(), model_scores.end());
        if (max_model_score > 70.0f) {
            reasons.push_back("ML model indicates high risk (score: " + 
                            std::to_string(static_cast<int>(max_model_score)) + ")");
        }
    }
    
    if (reasons.empty()) {
        reasons.push_back("Transaction within normal risk parameters");
    }
    
    return reasons;
}

bool DecisionContext::is_complete() const {
    return request.is_valid() &&
           features.size() == FEATURE_VECTOR_SIZE &&
           (!rule_scores.empty() || !model_scores.empty());
}

// FeatureSet implementation
bool FeatureSet::is_fresh(uint64_t max_age_ms) const {
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return (now - computed_at) <= max_age_ms;
}

std::vector<uint8_t> FeatureSet::serialize() const {
    std::vector<uint8_t> data;
    data.reserve(sizeof(computed_at) + sizeof(version) + 
                 values.size() * sizeof(float) + cache_key.size());
    
    // Serialize metadata
    auto* computed_at_bytes = reinterpret_cast<const uint8_t*>(&computed_at);
    data.insert(data.end(), computed_at_bytes, computed_at_bytes + sizeof(computed_at));
    
    auto* version_bytes = reinterpret_cast<const uint8_t*>(&version);
    data.insert(data.end(), version_bytes, version_bytes + sizeof(version));
    
    // Serialize feature values
    for (float value : values) {
        auto* value_bytes = reinterpret_cast<const uint8_t*>(&value);
        data.insert(data.end(), value_bytes, value_bytes + sizeof(value));
    }
    
    // Serialize cache key length and data
    uint32_t key_length = static_cast<uint32_t>(cache_key.size());
    auto* key_length_bytes = reinterpret_cast<const uint8_t*>(&key_length);
    data.insert(data.end(), key_length_bytes, key_length_bytes + sizeof(key_length));
    
    data.insert(data.end(), cache_key.begin(), cache_key.end());
    
    return data;
}

Result<FeatureSet> FeatureSet::deserialize(const std::vector<uint8_t>& data) {
    FeatureSet feature_set;
    
    if (data.size() < sizeof(uint64_t) + sizeof(uint32_t) + 
                    FEATURE_VECTOR_SIZE * sizeof(float) + sizeof(uint32_t)) {
        return {feature_set, ErrorCode::INVALID_REQUEST, "Insufficient data for deserialization"};
    }
    
    size_t offset = 0;
    
    // Deserialize metadata
    std::memcpy(&feature_set.computed_at, data.data() + offset, sizeof(uint64_t));
    offset += sizeof(uint64_t);
    
    std::memcpy(&feature_set.version, data.data() + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    
    // Deserialize feature values
    for (size_t i = 0; i < FEATURE_VECTOR_SIZE; ++i) {
        if (offset + sizeof(float) > data.size()) {
            return {feature_set, ErrorCode::INVALID_REQUEST, "Insufficient data for feature values"};
        }
        std::memcpy(&feature_set.values[i], data.data() + offset, sizeof(float));
        offset += sizeof(float);
    }
    
    // Deserialize cache key
    if (offset + sizeof(uint32_t) > data.size()) {
        return {feature_set, ErrorCode::INVALID_REQUEST, "Insufficient data for cache key length"};
    }
    
    uint32_t key_length;
    std::memcpy(&key_length, data.data() + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    
    if (offset + key_length > data.size()) {
        return {feature_set, ErrorCode::INVALID_REQUEST, "Insufficient data for cache key"};
    }
    
    feature_set.cache_key.assign(data.begin() + offset, data.begin() + offset + key_length);
    
    return {feature_set, ErrorCode::SUCCESS, ""};
}

} // namespace dmp
