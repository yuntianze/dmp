#include "engine/rule_engine.hpp"
#include "common/types.hpp"
#include "utils/logger.hpp"
#include <exprtk.hpp>
#include <simdjson.h>
#include <fstream>
#include <filesystem>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <chrono>
#include <unordered_map>
#include <algorithm>
#include <memory_resource>

namespace dmp {

// Thread-local storage for compiled expressions
thread_local static std::unordered_map<std::string, std::unique_ptr<exprtk::expression<double>>> tl_compiled_rules;
thread_local static exprtk::symbol_table<double> tl_symbol_table;
thread_local static bool tl_symbol_table_initialized = false;

// Memory pool for reducing allocations
thread_local static std::pmr::unsynchronized_pool_resource tl_memory_pool;

/**
 * Implementation class using pimpl idiom for ExprTk integration
 */
class RuleEngine::Impl {
public:
    mutable std::shared_mutex config_mutex_;
    std::atomic<bool> initialized_{false};
    std::atomic<bool> hot_reload_enabled_{false};
    std::string config_path_;
    std::string last_error_;
    
    RuleConfig current_config_;
    std::filesystem::file_time_type last_file_time_;
    
    // Hot reload thread
    std::unique_ptr<std::thread> hot_reload_thread_;
    std::atomic<bool> stop_hot_reload_{false};
    HotReloadCallback reload_callback_;
    uint32_t check_interval_ms_{5000};
    
    // Performance statistics
    mutable std::mutex stats_mutex_;
    std::unordered_map<std::string, Rule> rule_stats_;
    
    Impl() = default;
    
    ~Impl() {
        disable_hot_reload();
    }
    
    void disable_hot_reload() {
        if (hot_reload_enabled_.load()) {
            stop_hot_reload_.store(true);
            hot_reload_enabled_.store(false);
            if (hot_reload_thread_ && hot_reload_thread_->joinable()) {
                hot_reload_thread_->join();
            }
            hot_reload_thread_.reset();
        }
    }
    
    Result<void> load_rules_from_file(const std::string& config_path) {
        try {
            // Check if file exists
            if (!std::filesystem::exists(config_path)) {
                return Result<void>{ErrorCode::INVALID_REQUEST, 
                    "Configuration file does not exist: " + config_path};
            }
            
            // Read file content
            std::ifstream file(config_path);
            if (!file.is_open()) {
                return Result<void>{ErrorCode::INVALID_REQUEST, 
                    "Cannot open configuration file: " + config_path};
            }
            
            std::string content((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());
            file.close();
            
            // Parse JSON using simdjson
            simdjson::dom::parser parser;
            simdjson::dom::element json_doc;
            auto parse_error = parser.parse(content).get(json_doc);
            if (parse_error) {
                return Result<void>{ErrorCode::INVALID_JSON_FORMAT,
                    "JSON parse error: " + std::string(simdjson::error_message(parse_error))};
            }
            
            // Create new configuration
            RuleConfig new_config;
            new_config.loaded_at = std::chrono::system_clock::now();
            last_file_time_ = std::filesystem::last_write_time(config_path);
            
            // Parse version
            std::string_view version_sv;
            if (json_doc["version"].get_string().get(version_sv) == simdjson::SUCCESS) {
                new_config.version = std::string(version_sv);
            } else {
                new_config.version = "1.0.0";
            }
            
            // Parse thresholds
            if (auto thresholds_obj = json_doc["thresholds"]; !thresholds_obj.error()) {
                double approve_threshold, review_threshold;
                if (thresholds_obj["approve_threshold"].get_double().get(approve_threshold) == simdjson::SUCCESS) {
                    new_config.thresholds.approve_threshold = static_cast<float>(approve_threshold);
                }
                if (thresholds_obj["review_threshold"].get_double().get(review_threshold) == simdjson::SUCCESS) {
                    new_config.thresholds.review_threshold = static_cast<float>(review_threshold);
                }
            }
            
            // Parse rules array
            simdjson::dom::array rules_array;
            if (json_doc["rules"].get_array().get(rules_array) != simdjson::SUCCESS) {
                return Result<void>{ErrorCode::INVALID_JSON_FORMAT, "Missing or invalid 'rules' array"};
            }
            
            new_config.rules.reserve(rules_array.size());
            
            for (auto rule_element : rules_array) {
                Rule rule = {};
                
                // Parse rule fields
                std::string_view id_sv, name_sv, expression_sv, description_sv;
                double weight;
                bool enabled;
                
                if (rule_element["id"].get_string().get(id_sv) != simdjson::SUCCESS) {
                    continue; // Skip invalid rules
                }
                rule.id = std::string(id_sv);
                
                if (rule_element["name"].get_string().get(name_sv) == simdjson::SUCCESS) {
                    rule.name = std::string(name_sv);
                }
                
                if (rule_element["expression"].get_string().get(expression_sv) != simdjson::SUCCESS) {
                    LOG_ERROR("Rule {} missing expression, skipping", rule.id);
                    continue;
                }
                rule.expression = std::string(expression_sv);
                
                if (rule_element["weight"].get_double().get(weight) == simdjson::SUCCESS) {
                    rule.weight = static_cast<float>(weight);
                } else {
                    rule.weight = 1.0f; // Default weight
                }
                
                if (rule_element["enabled"].get_bool().get(enabled) == simdjson::SUCCESS) {
                    rule.enabled = enabled;
                } else {
                    rule.enabled = true; // Default enabled
                }
                
                if (rule_element["description"].get_string().get(description_sv) == simdjson::SUCCESS) {
                    rule.description = std::string(description_sv);
                }
                
                // Initialize statistics
                rule.hit_count = 0;
                rule.evaluation_count = 0;
                rule.total_evaluation_time_us = 0.0;
                
                new_config.rules.push_back(std::move(rule));
            }
            
            // Sort rules by weight (descending) for priority evaluation
            std::sort(new_config.rules.begin(), new_config.rules.end(),
                [](const Rule& a, const Rule& b) { return a.weight > b.weight; });
            
            // Update configuration atomically
            {
                std::unique_lock<std::shared_mutex> lock(config_mutex_);
                current_config_ = std::move(new_config);
                config_path_ = config_path;
                
                // Update rule statistics
                std::lock_guard<std::mutex> stats_lock(stats_mutex_);
                for (const auto& rule : current_config_.rules) {
                    if (rule_stats_.find(rule.id) == rule_stats_.end()) {
                        rule_stats_[rule.id] = rule;
                        // Initialize statistics fields
                        rule_stats_[rule.id].hit_count = 0;
                        rule_stats_[rule.id].evaluation_count = 0;
                        rule_stats_[rule.id].total_evaluation_time_us = 0.0;
                    }
                }
            }
            
            // Clear thread-local cache to force recompilation
            tl_compiled_rules.clear();
            tl_symbol_table_initialized = false;
            
            initialized_.store(true);
            LOG_INFO("Loaded {} rules from {}", current_config_.rules.size(), config_path);
            
            return Result<void>{};
            
        } catch (const std::exception& e) {
            last_error_ = std::string("Exception loading rules: ") + e.what();
            return Result<void>{ErrorCode::INTERNAL_ERROR, last_error_};
        }
    }
    
    void initialize_symbol_table(const RuleContext& context) {
        // Always clear and reinitialize to ensure fresh values
        tl_symbol_table.clear();
        
        // Add all context variables to symbol table
        tl_symbol_table.add_variable("amount", const_cast<double&>(context.amount));
        tl_symbol_table.add_stringvar("currency", const_cast<std::string&>(context.currency));
        tl_symbol_table.add_stringvar("merchant_id", const_cast<std::string&>(context.merchant_id));
        double merchant_category_val = static_cast<double>(context.merchant_category);
        tl_symbol_table.add_variable("merchant_category", merchant_category_val);
        tl_symbol_table.add_stringvar("pos_entry_mode", const_cast<std::string&>(context.pos_entry_mode));
        
        tl_symbol_table.add_stringvar("card_token", const_cast<std::string&>(context.card_token));
        tl_symbol_table.add_stringvar("issuer_country", const_cast<std::string&>(context.issuer_country));
        tl_symbol_table.add_stringvar("card_brand", const_cast<std::string&>(context.card_brand));
        
        tl_symbol_table.add_stringvar("ip_address", const_cast<std::string&>(context.ip_address));
        tl_symbol_table.add_stringvar("device_fingerprint", const_cast<std::string&>(context.device_fingerprint));
        tl_symbol_table.add_stringvar("user_agent", const_cast<std::string&>(context.user_agent));
        
        tl_symbol_table.add_stringvar("customer_id", const_cast<std::string&>(context.customer_id));
        double customer_risk_score_val = static_cast<double>(context.customer_risk_score);
        tl_symbol_table.add_variable("customer_risk_score", customer_risk_score_val);
        double account_age_days_val = static_cast<double>(context.account_age_days);
        tl_symbol_table.add_variable("account_age_days", account_age_days_val);
        
        // Derived fields
        double merchant_risk_val = static_cast<double>(context.merchant_risk);
        tl_symbol_table.add_variable("merchant_risk", merchant_risk_val);
        double hourly_count_val = static_cast<double>(context.hourly_count);
        tl_symbol_table.add_variable("hourly_count", hourly_count_val);
        tl_symbol_table.add_variable("amount_sum", const_cast<double&>(context.amount_sum));
        double ip_blacklist_match_val = static_cast<double>(context.ip_blacklist_match ? 1.0 : 0.0);
        tl_symbol_table.add_variable("ip_blacklist_match", ip_blacklist_match_val);
        
        tl_symbol_table_initialized = true;
    }
    
    std::unique_ptr<exprtk::expression<double>> compile_rule(const std::string& rule_id, 
                                                           const std::string& expression) {
        auto compiled_expr = std::make_unique<exprtk::expression<double>>();
        compiled_expr->register_symbol_table(tl_symbol_table);
        
        exprtk::parser<double> parser;
        if (!parser.compile(expression, *compiled_expr)) {
            LOG_ERROR("Failed to compile rule {}: {}", rule_id, parser.error());
            return nullptr;
        }
        
        return compiled_expr;
    }
    
    void hot_reload_worker() {
        LOG_INFO("Hot reload thread started, checking every {}ms", check_interval_ms_);
        
        while (!stop_hot_reload_.load()) {
            try {
                std::this_thread::sleep_for(std::chrono::milliseconds(check_interval_ms_));
                
                if (stop_hot_reload_.load()) {
                    break;
                }
                
                // Check if file has been modified
                if (!std::filesystem::exists(config_path_)) {
                    LOG_ERROR("Configuration file {} no longer exists", config_path_);
                    continue;
                }
                
                auto current_file_time = std::filesystem::last_write_time(config_path_);
                if (current_file_time != last_file_time_) {
                    LOG_INFO("Configuration file {} modified, reloading rules", config_path_);
                    
                    auto result = load_rules_from_file(config_path_);
                    if (result.is_success()) {
                        LOG_INFO("Rules reloaded successfully");
                        
                        // Call reload callback if provided
                        if (reload_callback_) {
                            try {
                                RuleConfig config_copy;
                                {
                                    std::shared_lock<std::shared_mutex> lock(config_mutex_);
                                    config_copy = current_config_;
                                }
                                reload_callback_(config_copy);
                            } catch (const std::exception& e) {
                                LOG_ERROR("Hot reload callback failed: {}", e.what());
                            }
                        }
                    } else {
                        LOG_ERROR("Failed to reload rules: {}", result.error_message);
                    }
                }
                
            } catch (const std::exception& e) {
                LOG_ERROR("Hot reload thread error: {}", e.what());
                std::this_thread::sleep_for(std::chrono::seconds(1)); // Prevent tight loop
            }
        }
        
        LOG_INFO("Hot reload thread stopped");
    }
};

// RuleContext implementation
RuleContext RuleContext::from_transaction(const TransactionRequest& request) {
    RuleContext context = {};
    
    // Transaction fields
    context.amount = request.transaction.amount;
    context.currency = request.transaction.currency;
    context.merchant_id = request.transaction.merchant_id;
    context.merchant_category = request.transaction.merchant_category;
    context.pos_entry_mode = request.transaction.pos_entry_mode;
    
    // Card fields
    context.card_token = request.card.token;
    context.issuer_country = request.card.issuer_country;
    context.card_brand = request.card.card_brand;
    
    // Device fields
    context.ip_address = request.device.ip;
    context.device_fingerprint = request.device.fingerprint;
    context.user_agent = request.device.user_agent;
    
    // Customer fields
    context.customer_id = request.customer.id;
    context.customer_risk_score = request.customer.risk_score;
    context.account_age_days = request.customer.account_age_days;
    
    // Initialize derived fields to default values
    // These would normally be computed by feature extraction
    context.merchant_risk = 0.0f;
    context.hourly_count = 1;
    context.amount_sum = context.amount;
    context.ip_blacklist_match = 0;
    
    return context;
}

bool RuleContext::is_valid() const {
    return !customer_id.empty() && !merchant_id.empty() && 
           !currency.empty() && amount > 0.0;
}

// RuleEngine implementation
RuleEngine::RuleEngine() : pimpl_(std::make_unique<Impl>()) {
    LOG_DEBUG("RuleEngine constructed");
}

RuleEngine::~RuleEngine() {
    LOG_DEBUG("RuleEngine destructed");
}

Result<void> RuleEngine::load_rules(const std::string& config_path) {
    LOG_INFO("Loading rules from {}", config_path);
    return pimpl_->load_rules_from_file(config_path);
}

Result<void> RuleEngine::enable_hot_reload(uint32_t check_interval_ms, HotReloadCallback callback) {
    if (pimpl_->hot_reload_enabled_.load()) {
        return Result<void>{ErrorCode::INVALID_REQUEST, "Hot reload already enabled"};
    }
    
    if (!pimpl_->initialized_.load()) {
        return Result<void>{ErrorCode::INVALID_REQUEST, "Rules not loaded yet"};
    }
    
    pimpl_->check_interval_ms_ = check_interval_ms;
    pimpl_->reload_callback_ = std::move(callback);
    pimpl_->stop_hot_reload_.store(false);
    
    try {
        pimpl_->hot_reload_thread_ = std::make_unique<std::thread>(&Impl::hot_reload_worker, pimpl_.get());
        pimpl_->hot_reload_enabled_.store(true);
        LOG_INFO("Hot reload enabled with {}ms interval", check_interval_ms);
        return Result<void>{};
    } catch (const std::exception& e) {
        return Result<void>{ErrorCode::INTERNAL_ERROR, 
            std::string("Failed to start hot reload thread: ") + e.what()};
    }
}

void RuleEngine::disable_hot_reload() {
    pimpl_->disable_hot_reload();
    LOG_INFO("Hot reload disabled");
}

RuleEvaluationMetrics RuleEngine::evaluate_rules(const TransactionRequest& request) {
    RuleEvaluationMetrics metrics = {};
    metrics.start_time = std::chrono::steady_clock::now();
    
    if (!pimpl_->initialized_.load()) {
        LOG_ERROR("Rule engine not initialized");
        metrics.end_time = std::chrono::steady_clock::now();
        return metrics;
    }
    
    // Create rule context from transaction
    RuleContext context = RuleContext::from_transaction(request);
    if (!context.is_valid()) {
        LOG_ERROR("Invalid rule context for request {}", request.request_id);
        metrics.end_time = std::chrono::steady_clock::now();
        return metrics;
    }
    
    // Initialize thread-local symbol table
    pimpl_->initialize_symbol_table(context);
    
    // Get current enabled rules
    std::vector<Rule> enabled_rules;
    {
        std::shared_lock<std::shared_mutex> lock(pimpl_->config_mutex_);
        enabled_rules = pimpl_->current_config_.get_enabled_rules();
    }
    
    metrics.rule_results.reserve(enabled_rules.size());
    metrics.rules_evaluated = enabled_rules.size();
    
    // Evaluate each rule
    for (const auto& rule : enabled_rules) {
        auto rule_start = std::chrono::high_resolution_clock::now();
        
        try {
            // Get or compile expression
            auto it = tl_compiled_rules.find(rule.id);
            if (it == tl_compiled_rules.end()) {
                auto compiled = pimpl_->compile_rule(rule.id, rule.expression);
                if (!compiled) {
                    LOG_ERROR("Failed to compile rule {}, skipping", rule.id);
                    // Initialize statistics even for failed rules
                    {
                        std::lock_guard<std::mutex> lock(pimpl_->stats_mutex_);
                        if (pimpl_->rule_stats_.find(rule.id) == pimpl_->rule_stats_.end()) {
                            pimpl_->rule_stats_[rule.id] = rule;
                        }
                    }
                    continue;
                }
                it = tl_compiled_rules.emplace(rule.id, std::move(compiled)).first;
            }
            
            // Evaluate expression
            double result = it->second->value();
            bool triggered = (result > 0.5); // Boolean result converted to double
            
            auto rule_end = std::chrono::high_resolution_clock::now();
            double evaluation_time_us = std::chrono::duration<double, std::micro>(
                rule_end - rule_start).count();
            
            // Create rule result
            RuleResult rule_result(rule.id, triggered, 
                triggered ? rule.weight : 0.0f, evaluation_time_us);
            
            if (triggered) {
                metrics.total_score += rule.weight;
                metrics.rules_triggered++;
                rule_result.debug_info = "Rule triggered with result: " + std::to_string(result);
            }
            
            metrics.rule_results.push_back(std::move(rule_result));
            metrics.total_evaluation_time_us += evaluation_time_us;
            
            // Update rule statistics
            {
                std::lock_guard<std::mutex> lock(pimpl_->stats_mutex_);
                auto& stats = pimpl_->rule_stats_[rule.id];
                stats.evaluation_count++;
                stats.total_evaluation_time_us += evaluation_time_us;
                if (triggered) {
                    stats.hit_count++;
                }
            }
            
        } catch (const std::exception& e) {
            LOG_ERROR("Error evaluating rule {}: {}", rule.id, e.what());
            // Continue with other rules
        }
    }
    
    metrics.end_time = std::chrono::steady_clock::now();
    
    LOG_DEBUG("Evaluated {} rules for request {}, score: {:.2f}, triggered: {}, latency: {:.2f}ms",
        metrics.rules_evaluated, request.request_id, metrics.total_score,
        metrics.rules_triggered, metrics.get_latency_ms());
    
    return metrics;
}

RuleConfig RuleEngine::get_current_config() const {
    std::shared_lock<std::shared_mutex> lock(pimpl_->config_mutex_);
    return pimpl_->current_config_;
}

std::unordered_map<std::string, Rule> RuleEngine::get_rule_statistics() const {
    std::lock_guard<std::mutex> lock(pimpl_->stats_mutex_);
    return pimpl_->rule_stats_;
}

void RuleEngine::reset_statistics() {
    std::lock_guard<std::mutex> lock(pimpl_->stats_mutex_);
    for (auto& [rule_id, stats] : pimpl_->rule_stats_) {
        stats.hit_count = 0;
        stats.evaluation_count = 0;
        stats.total_evaluation_time_us = 0.0;
    }
    LOG_INFO("Rule statistics reset");
}

bool RuleEngine::is_initialized() const {
    return pimpl_->initialized_.load();
}

std::string RuleEngine::get_last_error() const {
    return pimpl_->last_error_;
}

} // namespace dmp
