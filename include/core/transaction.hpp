#pragma once

#include "common/types.hpp"
#include <simdjson.h>

namespace dmp {

// 交易信息结构
struct TransactionInfo {
    Amount amount;
    std::string currency;
    MerchantId merchant_id;
    uint16_t merchant_category;
    std::string pos_entry_mode;
};

// 卡片信息结构
struct CardInfo {
    std::string token;
    std::string issuer_country;
    std::string card_brand;
};

// 设备信息结构
struct DeviceInfo {
    std::string ip;
    std::string fingerprint;
    std::string user_agent;
};

// 客户信息结构
struct CustomerInfo {
    UserId id;
    RiskScore risk_score;
    uint32_t account_age_days;
};

// 完整交易请求
struct TransactionRequest {
    RequestId request_id;
    Timestamp timestamp;
    TransactionInfo transaction;
    CardInfo card;
    DeviceInfo device;
    CustomerInfo customer;
    
    // 解析方法
    static Result<TransactionRequest> from_json(const simdjson::dom::element& json);
    
    // 验证方法
    bool is_valid() const;
    
    // 特征提取关键字段
    std::string get_cache_key() const;
};

// 交易响应
struct TransactionResponse {
    RequestId request_id;
    Decision decision;
    RiskScore risk_score;
    std::vector<std::string> triggered_rules;
    float latency_ms;
    std::string model_version;
    Timestamp timestamp;
    
    // 序列化方法
    std::string to_json() const;
};

// 内部决策上下文
struct DecisionContext {
    TransactionRequest request;
    FixedFeatureVector features;
    std::vector<float> rule_scores;
    std::vector<float> model_scores;
    
    // 计算最终风险分数
    RiskScore calculate_final_score() const;
    
    // 生成决策原因
    std::vector<std::string> generate_reasons() const;
};

} // namespace dmp
