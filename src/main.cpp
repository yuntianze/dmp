/**
 * @file main.cpp
 * @brief Main entry point for DMP risk control system (Phase 1 - Simplified)
 * @author Stan Jiang
 * @date 2025-08-28
 */

#include <iostream>
#include <memory>
#include <signal.h>
#include <filesystem>
#include <thread>
#include <chrono>
#include <iomanip>
#include <simdjson.h>
#include "common/types.hpp"
#include "common/config.hpp"
#include "core/transaction.hpp"
#include "utils/logger.hpp"

using namespace dmp;

// Global flag for graceful shutdown
std::atomic<bool> shutdown_requested{false};

/**
 * @brief Signal handler for graceful shutdown
 */
void signal_handler(int signal) {
    LOG_INFO("Received signal {}, initiating graceful shutdown...", signal);
    shutdown_requested.store(true);
}

// Old log_info function removed - using spdlog now

/**
 * @brief Initialize system components (Phase 1 simplified version)
 * @param config System configuration
 * @return Success or failure
 */
bool initialize_system(std::shared_ptr<SystemConfig> config) {
    try {
        // Set global configuration
        set_system_config(config);
        
        auto server_config = config->get_server_config();
        auto logging_config = config->get_logging_config();
        
        LOG_INFO("🚀 Starting DMP Risk Control System (Phase 1)");
        LOG_INFO("📋 Performance targets: P99 ≤ {}ms, QPS ≥ {}", 
                server_config.target_p99_ms, server_config.target_qps);
        LOG_INFO("🔧 Configuration loaded successfully");
        
        // Validate core data structures
        LOG_INFO("🔍 Testing core data structures...");
        
        // Test TransactionRequest parsing
        std::string test_json = R"({
            "request_id": "test_001",
            "timestamp": 1703001234567,
            "transaction": {
                "amount": 100.0,
                "currency": "USD",
                "merchant_id": "MERCH_001",
                "merchant_category": 5411,
                "pos_entry_mode": "CHIP"
            },
            "card": {
                "token": "tok_test",
                "issuer_country": "US",
                "card_brand": "VISA"
            },
            "device": {
                "ip": "192.168.1.1",
                "fingerprint": "fp_test",
                "user_agent": "Test/1.0"
            },
            "customer": {
                "id": "cust_001",
                "risk_score": 25.0,
                "account_age_days": 365
            }
        })";
        
        // Parse test transaction
        simdjson::dom::parser parser;
        auto json_doc_result = parser.parse(test_json);
        if (json_doc_result.error()) {
            std::cerr << "❌ JSON parsing test failed" << std::endl;
            return false;
        }
        
        auto json_doc = json_doc_result.value();
        auto transaction_result = TransactionRequest::from_json(json_doc);
        if (transaction_result.is_error()) {
            std::cerr << "❌ Transaction parsing test failed: " 
                      << transaction_result.error_message << std::endl;
            return false;
        }
        
        auto& transaction = transaction_result.value;
        if (!transaction.is_valid()) {
            std::cerr << "❌ Transaction validation test failed" << std::endl;
            return false;
        }
        
        LOG_INFO("✅ Transaction parsing test passed");
        LOG_INFO("✅ Cache key generation: {}", transaction.get_cache_key());
        
        // Test response serialization
        TransactionResponse response;
        response.request_id = transaction.request_id;
        response.decision = Decision::APPROVE;
        response.risk_score = 15.5f;
        response.triggered_rules = {"RULE_TEST"};
        response.latency_ms = 10.0f;
        response.model_version = "v1.0.0";
        response.timestamp = transaction.timestamp;
        
        std::string response_json = response.to_json();
        if (response_json.empty()) {
            std::cerr << "❌ Response serialization test failed" << std::endl;
            return false;
        }
        
        LOG_INFO("✅ Response serialization test passed");
        
        LOG_INFO("✅ All core components validated successfully");
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ System initialization failed: " << e.what() << std::endl;
        return false;
    }
}

/**
 * @brief Print startup banner
 */
void print_banner() {
    std::cout << R"(
    ____  __  __ ____    ____  _     _        ____            _             _ 
   |  _ \|  \/  |  _ \  |  _ \(_)___| | __   / ___|___  _ __ | |_ _ __ ___ | |
   | | | | |\/| | |_) | | |_) | / __| |/ /  | |   / _ \| '_ \| __| '__/ _ \| |
   | |_| | |  | |  __/  |  _ <| \__ \   <   | |__| (_) | | | | |_| | | (_) | |
   |____/|_|  |_|_|     |_| \_\_|___/_|\_\   \____\___/|_| |_|\__|_|  \___/|_|
                                                                              
)" << std::endl;
    
    std::cout << "  🎯 High-Performance Real-time Risk Control System" << std::endl;
    std::cout << "  📊 Target: P99 ≤ 50ms, QPS ≥ 10,000" << std::endl;
    std::cout << "  🔧 Built with modern C++20 and optimized libraries" << std::endl;
    std::cout << std::endl;
}

/**
 * @brief Main entry point (Phase 1 simplified version)
 */
int main(int argc, char* argv[]) {
    print_banner();
    
    try {
        // Initialize logging system first
        if (!Logger::initialize()) {
            std::cerr << "❌ Failed to initialize logging system" << std::endl;
            return 1;
        }
        
        // Setup signal handlers for graceful shutdown
        signal(SIGINT, signal_handler);
        signal(SIGTERM, signal_handler);
        
        // Determine configuration file path
        std::string config_path = "config/server.toml";
        if (argc > 1) {
            config_path = argv[1];
        }
        
        LOG_INFO("📁 Loading configuration from: {}", config_path);
        
        // Load system configuration
        auto config_result = SystemConfig::load_from_file(config_path);
        if (config_result.is_error()) {
            std::cerr << "❌ Failed to load configuration from " << config_path 
                      << ": " << config_result.error_message << std::endl;
            return 1;
        }
        
        auto config = config_result.value;
        
        // Validate configuration
        if (!config->is_valid()) {
            std::cerr << "❌ Invalid configuration detected" << std::endl;
            return 1;
        }
        
        // Initialize system components
        if (!initialize_system(config)) {
            std::cerr << "❌ System initialization failed" << std::endl;
            return 1;
        }
        
        LOG_INFO("📝 Phase 1 Summary:");
        LOG_INFO("  ✅ Configuration management (TOML parsing, validation)");
        LOG_INFO("  ✅ Core data structures (Transaction, Decision, Features)");
        LOG_INFO("  ✅ JSON serialization/deserialization");
        LOG_INFO("  ✅ Result template and error handling");
        LOG_INFO("  ✅ Cache key generation");
        LOG_INFO("  🚧 HTTP server (placeholder - will be added in Phase 2)");
        LOG_INFO("  🚧 Metrics collection (placeholder - will be added in Phase 2)");
        
        // Run main loop (simplified version)
        LOG_INFO("🔄 Running system validation loop...");
        int test_cycles = 0;
        
        while (!shutdown_requested.load() && test_cycles < 10) {
            LOG_INFO("🔍 Validation cycle {}", test_cycles + 1);
            
            // Test configuration reload
            auto reload_result = SystemConfig::load_from_file(config_path);
            if (reload_result.is_error()) {
                LOG_INFO("⚠️  Configuration reload test failed");
            } else {
                LOG_INFO("✅ Configuration reload test passed");
            }
            
            // Sleep for a bit
            std::this_thread::sleep_for(std::chrono::seconds(2));
            test_cycles++;
        }
        
        if (shutdown_requested.load()) {
            LOG_INFO("🛑 Graceful shutdown requested");
        } else {
            LOG_INFO("🏁 Phase 1 validation completed successfully");
        }
        
        LOG_INFO("✅ Ready for Phase 2 development");
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Fatal error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "❌ Unknown fatal error occurred" << std::endl;
        return 1;
    }
    
    return 0;
}
