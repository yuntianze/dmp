/**
 * @file rule_engine.hpp
 * @brief High-performance rule engine interface for DMP risk control system
 * @author Stan Jiang
 * @date 2025-08-28
 */
#pragma once

#include "common/types.hpp"
#include "core/transaction.hpp"
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <shared_mutex>
#include <functional>
#include <unordered_map>
#include <chrono>

namespace dmp {

/**
 * @brief Individual rule definition with metadata
 * 
 * Contains all information needed to evaluate a single risk rule,
 * including the expression, weight, and performance metrics.
 */
struct Rule {
    std::string id;                    // Unique rule identifier
    std::string name;                  // Human-readable rule name
    std::string expression;            // ExprTk expression string
    float weight;                      // Rule weight for scoring (0.0-100.0)
    bool enabled;                      // Whether rule is active
    std::string description;           // Rule description for audit
    uint64_t hit_count;               // Number of times rule was triggered
    uint64_t evaluation_count;        // Number of times rule was evaluated
    double total_evaluation_time_us;  // Total evaluation time in microseconds
    
    /**
     * @brief Calculate rule hit rate percentage
     * @return Hit rate as percentage (0.0-100.0)
     */
    double get_hit_rate() const {
        return evaluation_count > 0 ? 
            (static_cast<double>(hit_count) / evaluation_count) * 100.0 : 0.0;
    }
    
    /**
     * @brief Calculate average evaluation time
     * @return Average time per evaluation in microseconds
     */
    double get_avg_evaluation_time_us() const {
        return evaluation_count > 0 ? 
            total_evaluation_time_us / evaluation_count : 0.0;
    }
};

/**
 * @brief Result of rule evaluation for a single rule
 * 
 * Contains the evaluation outcome and performance metrics
 * for debugging and monitoring purposes.
 */
struct RuleResult {
    std::string rule_id;        // Rule that was evaluated
    bool triggered;             // Whether rule condition was met
    float contribution_score;   // Score contribution if triggered
    double evaluation_time_us;  // Time taken for this evaluation
    std::string debug_info;     // Additional debug information
    
    RuleResult() : triggered(false), contribution_score(0.0f), evaluation_time_us(0.0) {}
    
    RuleResult(const std::string& id, bool hit, float score, double time_us) 
        : rule_id(id), triggered(hit), contribution_score(score), evaluation_time_us(time_us) {}
};

/**
 * @brief Complete rule evaluation metrics for a transaction
 * 
 * Aggregates results from all rules and provides overall
 * performance statistics for monitoring.
 */
struct RuleEvaluationMetrics {
    std::vector<RuleResult> rule_results;  // Individual rule results
    float total_score;                     // Aggregated risk score
    size_t rules_triggered;                // Number of triggered rules
    size_t rules_evaluated;                // Total number of rules evaluated
    double total_evaluation_time_us;       // Total evaluation time
    std::chrono::steady_clock::time_point start_time;  // Evaluation start time
    std::chrono::steady_clock::time_point end_time;    // Evaluation end time
    
    /**
     * @brief Calculate overall evaluation latency
     * @return Total latency in milliseconds
     */
    double get_latency_ms() const {
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time - start_time);
        return duration.count() / 1000.0;
    }
    
    /**
     * @brief Get list of triggered rule IDs
     * @return Vector of rule IDs that were triggered
     */
    std::vector<std::string> get_triggered_rules() const {
        std::vector<std::string> triggered;
        for (const auto& result : rule_results) {
            if (result.triggered) {
                triggered.push_back(result.rule_id);
            }
        }
        return triggered;
    }
};

/**
 * @brief Rule configuration thresholds for decision making
 * 
 * Contains the score thresholds used to determine final
 * transaction decisions based on aggregated rule scores.
 */
struct RuleThresholds {
    float approve_threshold;  // Score below this = APPROVE
    float review_threshold;   // Score above this = DECLINE, between = REVIEW
    
    RuleThresholds() : approve_threshold(30.0f), review_threshold(70.0f) {}
    
    RuleThresholds(float approve, float review) 
        : approve_threshold(approve), review_threshold(review) {}
    
    /**
     * @brief Determine decision based on risk score
     * @param score Risk score to evaluate
     * @return Decision enum value
     */
    Decision make_decision(float score) const {
        if (score < approve_threshold) {
            return Decision::APPROVE;
        } else if (score >= review_threshold) {
            return Decision::DECLINE;
        } else {
            return Decision::REVIEW;
        }
    }
};

/**
 * @brief Rule configuration loaded from JSON file
 * 
 * Complete rule configuration including all rules,
 * thresholds, and metadata from the configuration file.
 */
struct RuleConfig {
    std::string version;              // Configuration version
    std::vector<Rule> rules;          // List of all rules
    RuleThresholds thresholds;        // Decision thresholds
    std::chrono::system_clock::time_point loaded_at;  // Load timestamp
    
    /**
     * @brief Get enabled rules only
     * @return Vector of enabled rules
     */
    std::vector<Rule> get_enabled_rules() const {
        std::vector<Rule> enabled;
        for (const auto& rule : rules) {
            if (rule.enabled) {
                enabled.push_back(rule);
            }
        }
        return enabled;
    }
    
    /**
     * @brief Find rule by ID
     * @param rule_id Rule identifier to search for
     * @return Optional rule if found
     */
    std::optional<Rule> find_rule(const std::string& rule_id) const {
        for (const auto& rule : rules) {
            if (rule.id == rule_id) {
                return rule;
            }
        }
        return std::nullopt;
    }
};

/**
 * @brief High-performance rule engine with ExprTk backend
 * 
 * Provides thread-safe rule evaluation with caching, hot reloading,
 * and comprehensive monitoring. Optimized for sub-millisecond
 * evaluation of hundreds of rules.
 */
class RuleEngine {
public:
    /**
     * @brief Hot reload callback function type
     * 
     * Called when rules are successfully reloaded to notify
     * other components of configuration changes.
     */
    using HotReloadCallback = std::function<void(const RuleConfig&)>;
    
    /**
     * @brief Constructor
     */
    RuleEngine();
    
    /**
     * @brief Destructor - ensures clean shutdown
     */
    ~RuleEngine();
    
    // Non-copyable and non-movable for thread safety
    RuleEngine(const RuleEngine&) = delete;
    RuleEngine& operator=(const RuleEngine&) = delete;
    RuleEngine(RuleEngine&&) = delete;
    RuleEngine& operator=(RuleEngine&&) = delete;
    
    /**
     * @brief Load rules from JSON configuration file
     * @param config_path Path to rules.json file
     * @return Result indicating success or error details
     * 
     * Loads and compiles all rules from the configuration file.
     * This is a one-time setup operation, typically called at startup.
     */
    Result<void> load_rules(const std::string& config_path);
    
    /**
     * @brief Enable hot reloading of rule configuration
     * @param check_interval_ms Interval between file checks in milliseconds
     * @param callback Optional callback for reload notifications
     * @return Result indicating success or error details
     * 
     * Starts a background thread that monitors the configuration file
     * for changes and automatically reloads rules when detected.
     */
    Result<void> enable_hot_reload(uint32_t check_interval_ms = 5000, 
                                  HotReloadCallback callback = nullptr);
    
    /**
     * @brief Disable hot reloading and stop background thread
     */
    void disable_hot_reload();
    
    /**
     * @brief Evaluate all enabled rules against a transaction
     * @param request Transaction request to evaluate
     * @return Rule evaluation metrics with scores and performance data
     * 
     * This is the main evaluation function called for each transaction.
     * Performance target: < 5ms for 100+ rules.
     * Thread-safe: Yes, uses thread-local compiled rule instances.
     */
    RuleEvaluationMetrics evaluate_rules(const TransactionRequest& request);
    
    /**
     * @brief Get current rule configuration (thread-safe copy)
     * @return Current rule configuration
     */
    RuleConfig get_current_config() const;
    
    /**
     * @brief Get aggregated rule statistics
     * @return Map of rule ID to hit count, evaluation count, etc.
     */
    std::unordered_map<std::string, Rule> get_rule_statistics() const;
    
    /**
     * @brief Reset all rule statistics counters
     */
    void reset_statistics();
    
    /**
     * @brief Check if rule engine is properly initialized
     * @return true if loaded and ready for evaluation
     */
    bool is_initialized() const;
    
    /**
     * @brief Get last error message if initialization failed
     * @return Error message or empty string if no error
     */
    std::string get_last_error() const;

private:
    // Implementation details hidden to allow for ExprTk integration
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

/**
 * @brief Rule evaluation context for feature variable binding
 * 
 * Contains all variables that can be used in rule expressions,
 * extracted from transaction data and cached features.
 */
struct RuleContext {
    // Transaction fields
    double amount;                    // Transaction amount
    std::string currency;             // Currency code
    std::string merchant_id;          // Merchant identifier
    uint16_t merchant_category;       // Merchant category code
    std::string pos_entry_mode;       // POS entry mode
    
    // Card fields
    std::string card_token;           // Tokenized card number
    std::string issuer_country;       // Card issuer country
    std::string card_brand;           // Card brand (VISA, MC, etc.)
    
    // Device fields
    std::string ip_address;           // Device IP address
    std::string device_fingerprint;   // Device fingerprint
    std::string user_agent;           // Browser user agent
    
    // Customer fields
    std::string customer_id;          // Customer identifier
    float customer_risk_score;        // Customer base risk score
    uint32_t account_age_days;        // Account age in days
    
    // Derived/computed fields (for Phase 2 feature integration)
    float merchant_risk;              // Merchant risk score
    uint32_t hourly_count;           // Transactions in last hour
    double amount_sum;               // Amount sum in time window
    int ip_blacklist_match;          // IP blacklist match result
    
    /**
     * @brief Create rule context from transaction request
     * @param request Transaction request to convert
     * @return Populated rule context
     * 
     * Extracts all relevant fields from the transaction request
     * and prepares them for rule evaluation.
     */
    static RuleContext from_transaction(const TransactionRequest& request);
    
    /**
     * @brief Validate context completeness
     * @return true if all required fields are present
     */
    bool is_valid() const;
};

} // namespace dmp
