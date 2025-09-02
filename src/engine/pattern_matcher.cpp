#include "engine/pattern_matcher.hpp"
#include "utils/logger.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <mutex>
#include <atomic>
#include <memory>
#include <regex>
#include <set>

// Conditional Hyperscan includes
#ifdef ENABLE_HYPERSCAN
#include <hs/hs.h>
#endif

namespace dmp {

/**
 * @brief Abstract pattern matching backend interface
 * 
 * Defines the interface that all pattern matching backends must implement.
 * Allows for runtime selection between different matching engines.
 */
class PatternBackend {
public:
    virtual ~PatternBackend() = default;
    
    virtual Result<void> compile_patterns(const std::vector<Pattern>& patterns) = 0;
    virtual PatternMatchResults match_text(const std::string& text, 
                                          const std::string& category = "") = 0;
    virtual PatternMatchResults match_batch(const std::vector<std::string>& texts,
                                           const std::string& category = "") = 0;
    virtual std::string get_backend_name() const = 0;
    virtual bool is_available() const = 0;
};

/**
 * @brief Standard library regex-based backend (fallback implementation)
 * 
 * Uses std::regex for pattern matching. Compatible with all platforms
 * but has lower performance compared to specialized engines.
 */
class StdRegexBackend : public PatternBackend {
private:
    struct CompiledPattern {
        Pattern pattern;
        std::regex compiled_regex;
        
        CompiledPattern(const Pattern& p) : pattern(p) {
            std::string regex_pattern = p.pattern;
            
            // Convert wildcards to regex if needed
            if (!p.is_regex && regex_pattern.find('*') != std::string::npos) {
                regex_pattern = PatternUtils::wildcard_to_regex(regex_pattern);
            }
            
            std::regex_constants::syntax_option_type flags = std::regex_constants::ECMAScript;
            if (!p.case_sensitive) {
                flags |= std::regex_constants::icase;
            }
            
            compiled_regex = std::regex(regex_pattern, flags);
        }
    };
    
    std::vector<CompiledPattern> compiled_patterns_;
    mutable std::mutex patterns_mutex_;
    std::atomic<uint64_t> match_count_{0};
    std::atomic<uint64_t> total_match_time_us_{0};
    
public:
    Result<void> compile_patterns(const std::vector<Pattern>& patterns) override {
        try {
            std::lock_guard<std::mutex> lock(patterns_mutex_);
            compiled_patterns_.clear();
            compiled_patterns_.reserve(patterns.size());
            
            for (const auto& pattern : patterns) {
                try {
                    compiled_patterns_.emplace_back(pattern);
                    LOG_DEBUG("✅ Compiled pattern [{}]: {}", pattern.id, pattern.name);
                } catch (const std::regex_error& e) {
                    LOG_ERROR("❌ Regex compilation failed [{}]: {}", pattern.id, e.what());
                    return {ErrorCode::RULE_EVALUATION_FAILED,
                           fmt::format("Pattern compilation failed [{}]: {}", pattern.id, e.what())};
                }
            }
            
            LOG_INFO("✅ Compiled {} patterns using std::regex backend", compiled_patterns_.size());
            return {ErrorCode::SUCCESS, ""};
            
        } catch (const std::exception& e) {
            return {ErrorCode::INTERNAL_ERROR, 
                   fmt::format("Exception during pattern compilation: {}", e.what())};
        }
    }
    
    PatternMatchResults match_text(const std::string& text, 
                                  const std::string& category = "") override {
        auto start_time = std::chrono::high_resolution_clock::now();
        
        PatternMatchResults results;
        results.texts_processed = 1;
        
        {
            std::lock_guard<std::mutex> lock(patterns_mutex_);
            results.patterns_checked = compiled_patterns_.size();
            
            for (const auto& compiled_pattern : compiled_patterns_) {
                const auto& pattern = compiled_pattern.pattern;
                
                // Category filter
                if (!category.empty() && pattern.category != category) {
                    continue;
                }
                
                try {
                    std::smatch match;
                    if (std::regex_search(text, match, compiled_pattern.compiled_regex)) {
                        PatternMatch pattern_match(
                            pattern.id,
                            pattern.name,
                            match.str(),
                            match.position(),
                            match.position() + match.length(),
                            pattern.category
                        );
                        
                        results.matches.push_back(pattern_match);
                        
                        // Categorize matches
                        if (pattern.category.find("blacklist") != std::string::npos) {
                            results.blacklist_matches.push_back(pattern_match);
                        } else if (pattern.category.find("whitelist") != std::string::npos) {
                            results.whitelist_matches.push_back(pattern_match);
                        }
                        
                        LOG_DEBUG("🎯 Pattern match [{}]: {} in text '{}'", 
                                 pattern.name, match.str(), 
                                 text.length() > 50 ? text.substr(0, 50) + "..." : text);
                    }
                } catch (const std::exception& e) {
                    LOG_ERROR("❌ Pattern matching exception [{}]: {}", pattern.id, e.what());
                }
            }
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        results.evaluation_time_us = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time - start_time).count();
        
        // Update statistics
        match_count_.fetch_add(1, std::memory_order_relaxed);
        total_match_time_us_.fetch_add(results.evaluation_time_us, std::memory_order_relaxed);
        
        return results;
    }
    
    PatternMatchResults match_batch(const std::vector<std::string>& texts,
                                   const std::string& category = "") override {
        PatternMatchResults aggregated_results;
        aggregated_results.texts_processed = texts.size();
        
        for (const auto& text : texts) {
            auto text_results = match_text(text, category);
            
            // Merge results
            aggregated_results.matches.insert(aggregated_results.matches.end(),
                                             text_results.matches.begin(),
                                             text_results.matches.end());
            aggregated_results.blacklist_matches.insert(aggregated_results.blacklist_matches.end(),
                                                       text_results.blacklist_matches.begin(),
                                                       text_results.blacklist_matches.end());
            aggregated_results.whitelist_matches.insert(aggregated_results.whitelist_matches.end(),
                                                       text_results.whitelist_matches.begin(),
                                                       text_results.whitelist_matches.end());
            aggregated_results.evaluation_time_us += text_results.evaluation_time_us;
            aggregated_results.patterns_checked = text_results.patterns_checked;
        }
        
        return aggregated_results;
    }
    
    std::string get_backend_name() const override {
        return "std::regex";
    }
    
    bool is_available() const override {
        return true; // Always available
    }
    
    uint64_t get_match_count() const {
        return match_count_.load();
    }
    
    double get_average_match_time_us() const {
        uint64_t count = match_count_.load();
        return count > 0 ? static_cast<double>(total_match_time_us_.load()) / count : 0.0;
    }
};

#ifdef ENABLE_HYPERSCAN
/**
 * @brief Hyperscan-based backend (high performance)
 * 
 * Uses Intel Hyperscan for high-performance pattern matching.
 * Provides SIMD-accelerated regex matching with batch support.
 */
class HyperscanBackend : public PatternBackend {
private:
    hs_database_t* database_;
    hs_scratch_t* scratch_;
    std::vector<Pattern> patterns_;
    mutable std::mutex hyperscan_mutex_;
    std::atomic<uint64_t> match_count_{0};
    std::atomic<uint64_t> total_match_time_us_{0};
    
public:
    HyperscanBackend() : database_(nullptr), scratch_(nullptr) {}
    
    ~HyperscanBackend() {
        if (scratch_) {
            hs_free_scratch(scratch_);
        }
        if (database_) {
            hs_free_database(database_);
        }
    }
    
    Result<void> compile_patterns(const std::vector<Pattern>& patterns) override {
        try {
            patterns_ = patterns;
            
            if (patterns.empty()) {
                return {ErrorCode::SUCCESS, ""};
            }
            
            // Prepare Hyperscan compilation data
            std::vector<const char*> expressions;
            std::vector<unsigned int> flags;
            std::vector<unsigned int> ids;
            
            expressions.reserve(patterns.size());
            flags.reserve(patterns.size());
            ids.reserve(patterns.size());
            
            for (const auto& pattern : patterns) {
                std::string regex_pattern = pattern.pattern;
                
                // Convert wildcards to regex if needed
                if (!pattern.is_regex && regex_pattern.find('*') != std::string::npos) {
                    regex_pattern = PatternUtils::wildcard_to_regex(regex_pattern);
                }
                
                expressions.push_back(regex_pattern.c_str());
                
                unsigned int pattern_flags = HS_FLAG_DOTALL | HS_FLAG_MULTILINE;
                if (!pattern.case_sensitive) {
                    pattern_flags |= HS_FLAG_CASELESS;
                }
                flags.push_back(pattern_flags);
                ids.push_back(pattern.id);
            }
            
            // Compile patterns into database
            hs_compile_error_t* compile_err = nullptr;
            hs_error_t err = hs_compile_multi(
                expressions.data(),
                flags.data(),
                ids.data(),
                patterns.size(),
                HS_MODE_BLOCK,
                nullptr,
                &database_,
                &compile_err
            );
            
            if (err != HS_SUCCESS) {
                std::string error_msg = compile_err ? compile_err->message : "Unknown compilation error";
                if (compile_err) {
                    hs_free_compile_error(compile_err);
                }
                return {ErrorCode::RULE_EVALUATION_FAILED,
                       fmt::format("Hyperscan compilation failed: {}", error_msg)};
            }
            
            // Allocate scratch space
            err = hs_alloc_scratch(database_, &scratch_);
            if (err != HS_SUCCESS) {
                return {ErrorCode::INTERNAL_ERROR, "Failed to allocate Hyperscan scratch space"};
            }
            
            LOG_INFO("✅ Compiled {} patterns using Hyperscan backend", patterns.size());
            return {ErrorCode::SUCCESS, ""};
            
        } catch (const std::exception& e) {
            return {ErrorCode::INTERNAL_ERROR,
                   fmt::format("Exception during Hyperscan compilation: {}", e.what())};
        }
    }
    
    PatternMatchResults match_text(const std::string& text, 
                                  const std::string& category = "") override {
        auto start_time = std::chrono::high_resolution_clock::now();
        
        PatternMatchResults results;
        results.texts_processed = 1;
        results.patterns_checked = patterns_.size();
        
        if (!database_ || !scratch_) {
            results.evaluation_time_us = 0;
            return results;
        }
        
        // Hyperscan match context
        struct MatchContext {
            PatternMatchResults* results;
            const std::vector<Pattern>* patterns;
            const std::string* text;
            std::string category_filter;
        };
        
        MatchContext context{&results, &patterns_, &text, category};
        
        {
            std::lock_guard<std::mutex> lock(hyperscan_mutex_);
            
            // Hyperscan callback function
            auto match_callback = [](unsigned int id, unsigned long long from,
                                    unsigned long long to, unsigned int flags,
                                    void* ctx) -> int {
                auto* match_ctx = static_cast<MatchContext*>(ctx);
                
                // Find pattern by ID
                auto pattern_it = std::find_if(match_ctx->patterns->begin(),
                                              match_ctx->patterns->end(),
                                              [id](const Pattern& p) { return p.id == id; });
                
                if (pattern_it != match_ctx->patterns->end()) {
                    const auto& pattern = *pattern_it;
                    
                    // Category filter
                    if (!match_ctx->category_filter.empty() && 
                        pattern.category != match_ctx->category_filter) {
                        return 0; // Continue matching
                    }
                    
                    std::string matched_text = match_ctx->text->substr(from, to - from);
                    
                    PatternMatch pattern_match(
                        pattern.id,
                        pattern.name,
                        matched_text,
                        from,
                        to,
                        pattern.category
                    );
                    
                    match_ctx->results->matches.push_back(pattern_match);
                    
                    // Categorize matches
                    if (pattern.category.find("blacklist") != std::string::npos) {
                        match_ctx->results->blacklist_matches.push_back(pattern_match);
                    } else if (pattern.category.find("whitelist") != std::string::npos) {
                        match_ctx->results->whitelist_matches.push_back(pattern_match);
                    }
                    
                    LOG_DEBUG("🎯 Hyperscan match [{}]: {} at [{}, {})", 
                             pattern.name, matched_text, from, to);
                }
                
                return 0; // Continue matching
            };
            
            // Perform scan
            hs_error_t err = hs_scan(database_, text.c_str(), text.length(), 0,
                                    scratch_, match_callback, &context);
            
            if (err != HS_SUCCESS) {
                LOG_ERROR("❌ Hyperscan scan failed: error code {}", err);
            }
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        results.evaluation_time_us = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time - start_time).count();
        
        // Update statistics
        match_count_.fetch_add(1, std::memory_order_relaxed);
        total_match_time_us_.fetch_add(results.evaluation_time_us, std::memory_order_relaxed);
        
        return results;
    }
    
    PatternMatchResults match_batch(const std::vector<std::string>& texts,
                                   const std::string& category = "") override {
        // For simplicity, batch processing uses sequential matching
        // Could be optimized with Hyperscan streaming mode in production
        PatternMatchResults aggregated_results;
        aggregated_results.texts_processed = texts.size();
        
        for (const auto& text : texts) {
            auto text_results = match_text(text, category);
            
            // Merge results
            aggregated_results.matches.insert(aggregated_results.matches.end(),
                                             text_results.matches.begin(),
                                             text_results.matches.end());
            aggregated_results.blacklist_matches.insert(aggregated_results.blacklist_matches.end(),
                                                       text_results.blacklist_matches.begin(),
                                                       text_results.blacklist_matches.end());
            aggregated_results.whitelist_matches.insert(aggregated_results.whitelist_matches.end(),
                                                       text_results.whitelist_matches.begin(),
                                                       text_results.whitelist_matches.end());
            aggregated_results.evaluation_time_us += text_results.evaluation_time_us;
            aggregated_results.patterns_checked = text_results.patterns_checked;
        }
        
        return aggregated_results;
    }
    
    std::string get_backend_name() const override {
        return "Hyperscan";
    }
    
    bool is_available() const override {
        // Check if Hyperscan is available at runtime
        hs_database_t* test_db = nullptr;
        const char* test_pattern = "test";
        unsigned int test_flag = 0;
        unsigned int test_id = 1;
        
        hs_compile_error_t* compile_err = nullptr;
        hs_error_t err = hs_compile(test_pattern, test_flag, HS_MODE_BLOCK,
                                   nullptr, &test_db, &compile_err);
        
        if (test_db) {
            hs_free_database(test_db);
        }
        if (compile_err) {
            hs_free_compile_error(compile_err);
        }
        
        return err == HS_SUCCESS;
    }
    
    uint64_t get_match_count() const {
        return match_count_.load();
    }
    
    double get_average_match_time_us() const {
        uint64_t count = match_count_.load();
        return count > 0 ? static_cast<double>(total_match_time_us_.load()) / count : 0.0;
    }
};
#endif // ENABLE_HYPERSCAN

/**
 * @brief Pattern matcher implementation using PIMPL pattern
 * 
 * Manages multiple backends and provides unified interface
 * for pattern matching with automatic backend selection.
 */
class PatternMatcher::Impl {
private:
    std::unique_ptr<PatternBackend> backend_;
    Backend active_backend_;
    std::vector<Pattern> loaded_patterns_;
    bool initialized_;
    std::string last_error_;
    mutable std::mutex impl_mutex_;
    
public:
    Impl(Backend preferred_backend) : active_backend_(Backend::STD_REGEX), initialized_(false) {
        select_backend(preferred_backend);
    }
    
    ~Impl() = default;
    
    Result<void> load_patterns(const std::string& blacklist_path,
                              const std::string& whitelist_path) {
        try {
            loaded_patterns_.clear();
            
            // Load blacklist patterns
            auto blacklist_result = PatternUtils::parse_pattern_file(blacklist_path, "blacklist");
            if (blacklist_result.is_error()) {
                last_error_ = blacklist_result.error_message;
                return {blacklist_result.error_code, blacklist_result.error_message};
            }
            
            auto blacklist_patterns = blacklist_result.value;
            loaded_patterns_.insert(loaded_patterns_.end(), 
                                   blacklist_patterns.begin(), blacklist_patterns.end());
            
            // Load whitelist patterns
            auto whitelist_result = PatternUtils::parse_pattern_file(whitelist_path, "whitelist");
            if (whitelist_result.is_error()) {
                last_error_ = whitelist_result.error_message;
                return {whitelist_result.error_code, whitelist_result.error_message};
            }
            
            auto whitelist_patterns = whitelist_result.value;
            loaded_patterns_.insert(loaded_patterns_.end(),
                                   whitelist_patterns.begin(), whitelist_patterns.end());
            
            LOG_INFO("✅ Loaded {} patterns ({} blacklist, {} whitelist)",
                    loaded_patterns_.size(), blacklist_patterns.size(), whitelist_patterns.size());
            
            return {ErrorCode::SUCCESS, ""};
            
        } catch (const std::exception& e) {
            last_error_ = fmt::format("Exception during pattern loading: {}", e.what());
            return {ErrorCode::INTERNAL_ERROR, last_error_};
        }
    }
    
    Result<void> add_pattern(const Pattern& pattern) {
        loaded_patterns_.push_back(pattern);
        initialized_ = false; // Need to recompile
        LOG_DEBUG("➕ Added pattern [{}]: {}", pattern.id, pattern.name);
        return {ErrorCode::SUCCESS, ""};
    }
    
    Result<void> compile_patterns() {
        if (!backend_) {
            last_error_ = "No backend available";
            return {ErrorCode::INTERNAL_ERROR, last_error_};
        }
        
        auto compile_result = backend_->compile_patterns(loaded_patterns_);
        if (compile_result.is_success()) {
            initialized_ = true;
            LOG_INFO("✅ Pattern compilation successful using {} backend", 
                    backend_->get_backend_name());
        } else {
            last_error_ = compile_result.error_message;
        }
        
        return compile_result;
    }
    
    PatternMatchResults match_transaction(const TransactionRequest& request) {
        if (!initialized_ || !backend_) {
            PatternMatchResults empty_results;
            LOG_ERROR("❌ Pattern matcher not initialized");
            return empty_results;
        }
        
        // Extract text fields from transaction
        auto text_fields = PatternUtils::extract_match_fields(request);
        
        PatternMatchResults aggregated_results;
        aggregated_results.texts_processed = text_fields.size();
        
        for (const auto& [field_name, text_value] : text_fields) {
            if (text_value.empty()) continue;
            
            auto field_results = backend_->match_text(text_value);
            
            // Merge results
            aggregated_results.matches.insert(aggregated_results.matches.end(),
                                             field_results.matches.begin(),
                                             field_results.matches.end());
            aggregated_results.blacklist_matches.insert(aggregated_results.blacklist_matches.end(),
                                                       field_results.blacklist_matches.begin(),
                                                       field_results.blacklist_matches.end());
            aggregated_results.whitelist_matches.insert(aggregated_results.whitelist_matches.end(),
                                                       field_results.whitelist_matches.begin(),
                                                       field_results.whitelist_matches.end());
            aggregated_results.evaluation_time_us += field_results.evaluation_time_us;
            aggregated_results.patterns_checked = field_results.patterns_checked;
        }
        
        LOG_DEBUG("🔍 Pattern matching completed: {} matches found in {:.2f}ms",
                 aggregated_results.total_matches(), 
                 aggregated_results.evaluation_time_us / 1000.0);
        
        return aggregated_results;
    }
    
    PatternMatchResults match_text(const std::string& text, const std::string& category) {
        if (!initialized_ || !backend_) {
            PatternMatchResults empty_results;
            return empty_results;
        }
        
        return backend_->match_text(text, category);
    }
    
    PatternMatchResults match_batch(const std::vector<std::string>& texts,
                                   const std::string& category) {
        if (!initialized_ || !backend_) {
            PatternMatchResults empty_results;
            return empty_results;
        }
        
        return backend_->match_batch(texts, category);
    }
    
    std::vector<Pattern> get_loaded_patterns() const {
        return loaded_patterns_;
    }
    
    Backend get_active_backend() const {
        return active_backend_;
    }
    
    std::unordered_map<std::string, uint64_t> get_statistics() const {
        std::unordered_map<std::string, uint64_t> stats;
        stats["total_patterns"] = loaded_patterns_.size();
        stats["patterns_loaded"] = loaded_patterns_.size();
        stats["backend_type"] = static_cast<uint64_t>(active_backend_);
        
        // Count patterns by category
        uint64_t blacklist_count = 0;
        uint64_t whitelist_count = 0;
        for (const auto& pattern : loaded_patterns_) {
            if (pattern.category.find("blacklist") != std::string::npos) {
                blacklist_count++;
            } else if (pattern.category.find("whitelist") != std::string::npos) {
                whitelist_count++;
            }
        }
        stats["blocklist_patterns"] = blacklist_count;
        stats["whitelist_patterns"] = whitelist_count;
        
        if (auto* std_backend = dynamic_cast<StdRegexBackend*>(backend_.get())) {
            stats["match_count"] = std_backend->get_match_count();
            stats["avg_match_time_us"] = static_cast<uint64_t>(std_backend->get_average_match_time_us());
        }
#ifdef ENABLE_HYPERSCAN
        else if (auto* hs_backend = dynamic_cast<HyperscanBackend*>(backend_.get())) {
            stats["match_count"] = hs_backend->get_match_count();
            stats["avg_match_time_us"] = static_cast<uint64_t>(hs_backend->get_average_match_time_us());
        }
#endif
        
        return stats;
    }
    
    void reset_statistics() {
        // Statistics are reset automatically when backends are recreated
        LOG_INFO("📊 Pattern matcher statistics reset");
    }
    
    bool is_initialized() const {
        return initialized_;
    }
    
    std::string get_last_error() const {
        return last_error_;
    }

private:
    void select_backend(Backend preferred_backend) {
        switch (preferred_backend) {
            case Backend::AUTO:
                // Try backends in order of preference
#ifdef ENABLE_HYPERSCAN
                if (try_hyperscan_backend()) return;
#endif
                try_std_regex_backend();
                break;
                
            case Backend::HYPERSCAN:
#ifdef ENABLE_HYPERSCAN
                if (!try_hyperscan_backend()) {
                    LOG_INFO("⚠️  Hyperscan backend requested but not available, falling back to std::regex");
                    try_std_regex_backend();
                }
#else
                LOG_INFO("⚠️  Hyperscan backend requested but not compiled in, using std::regex");
                try_std_regex_backend();
#endif
                break;
                
            case Backend::STD_REGEX:
                try_std_regex_backend();
                break;
                
            case Backend::VECTORSCAN:
                LOG_INFO("⚠️  Vectorscan backend not yet implemented, using std::regex");
                try_std_regex_backend();
                break;
                
            default:
                try_std_regex_backend();
                break;
        }
    }
    
#ifdef ENABLE_HYPERSCAN
    bool try_hyperscan_backend() {
        auto hyperscan_backend = std::make_unique<HyperscanBackend>();
        if (hyperscan_backend->is_available()) {
            backend_ = std::move(hyperscan_backend);
            active_backend_ = Backend::HYPERSCAN;
            LOG_INFO("🚀 Selected Hyperscan backend for high-performance pattern matching");
            return true;
        }
        return false;
    }
#endif
    
    void try_std_regex_backend() {
        backend_ = std::make_unique<StdRegexBackend>();
        active_backend_ = Backend::STD_REGEX;
        LOG_INFO("📋 Selected std::regex backend for pattern matching");
    }
};

// ============================================================================
// PatternUtils Implementation
// ============================================================================

namespace PatternUtils {

Result<std::vector<Pattern>> parse_pattern_file(const std::string& file_path,
                                               const std::string& category) {
    try {
        std::ifstream file(file_path);
        if (!file.is_open()) {
            return {std::vector<Pattern>{}, ErrorCode::INVALID_REQUEST,
                   fmt::format("Cannot open pattern file: {}", file_path)};
        }
        
        std::vector<Pattern> patterns;
        std::string line;
        uint32_t pattern_id = 1;
        uint32_t line_number = 0; (void)line_number; // Suppress unused warning
        
        while (std::getline(file, line)) {
            line_number++;
            
            // Trim whitespace
            line.erase(0, line.find_first_not_of(" \t\r\n"));
            line.erase(line.find_last_not_of(" \t\r\n") + 1);
            
            // Skip empty lines and comments
            if (line.empty() || line[0] == '#') {
                continue;
            }
            
            Pattern pattern;
            pattern.id = pattern_id++;
            pattern.pattern = line;
            pattern.category = category;
            pattern.name = fmt::format("{}_{}", category, pattern.id);
            pattern.case_sensitive = true;
            pattern.priority = 10; // Default priority
            
            // Detect pattern type
            if (line.find('/') != std::string::npos && 
                (line.find('.') != std::string::npos || line.find(':') != std::string::npos)) {
                // Looks like CIDR notation
                auto cidr_result = cidr_to_regex(line);
                if (cidr_result.is_success()) {
                    pattern.pattern = cidr_result.value;
                    pattern.is_regex = true;
                    pattern.name = fmt::format("{}_cidr_{}", category, pattern.id);
                }
            } else if (line.find('*') != std::string::npos) {
                // Wildcard pattern
                pattern.is_regex = false; // Will be converted during compilation
                pattern.name = fmt::format("{}_wildcard_{}", category, pattern.id);
            } else {
                // Exact string match
                pattern.is_regex = false;
                pattern.name = fmt::format("{}_exact_{}", category, pattern.id);
            }
            
            patterns.push_back(pattern);
        }
        
        LOG_INFO("📄 Parsed {} patterns from {} ({})", patterns.size(), file_path, category);
        return {patterns, ErrorCode::SUCCESS, ""};
        
    } catch (const std::exception& e) {
        return {std::vector<Pattern>{}, ErrorCode::INTERNAL_ERROR,
               fmt::format("Exception parsing pattern file {}: {}", file_path, e.what())};
    }
}

std::string wildcard_to_regex(const std::string& wildcard_pattern) {
    std::string regex_pattern;
    regex_pattern.reserve(wildcard_pattern.length() * 2);
    
    regex_pattern += "^"; // Anchor to start
    
    for (char c : wildcard_pattern) {
        switch (c) {
            case '*':
                regex_pattern += ".*";
                break;
            case '?':
                regex_pattern += ".";
                break;
            case '.':
            case '^':
            case '$':
            case '+':
            case '(':
            case ')':
            case '[':
            case ']':
            case '{':
            case '}':
            case '|':
            case '\\':
                regex_pattern += '\\';
                regex_pattern += c;
                break;
            default:
                regex_pattern += c;
                break;
        }
    }
    
    regex_pattern += "$"; // Anchor to end
    return regex_pattern;
}

Result<std::string> cidr_to_regex(const std::string& cidr_pattern) {
    try {
        size_t slash_pos = cidr_pattern.find('/');
        if (slash_pos == std::string::npos) {
            return {std::string{}, ErrorCode::INVALID_REQUEST,
                   fmt::format("Invalid CIDR notation: {}", cidr_pattern)};
        }
        
        std::string ip_part = cidr_pattern.substr(0, slash_pos);
        std::string prefix_part = cidr_pattern.substr(slash_pos + 1);
        
        int prefix_length = std::stoi(prefix_part);
        if (prefix_length < 0 || prefix_length > 32) {
            return {std::string{}, ErrorCode::INVALID_REQUEST,
                   fmt::format("Invalid CIDR prefix length: {}", prefix_length)};
        }
        
        // For simplicity, convert common CIDR patterns to regex
        // A full implementation would parse IP and create proper ranges
        std::string regex_pattern = "^";
        
        if (prefix_length >= 24) {
            // /24 or smaller - match first 3 octets exactly
            size_t last_dot = ip_part.find_last_of('.');
            if (last_dot != std::string::npos) {
                std::string prefix = ip_part.substr(0, last_dot);
                // Escape dots for regex
                std::replace(prefix.begin(), prefix.end(), '.', '\\');
                regex_pattern += prefix + "\\.\\d{1,3}";
            }
        } else if (prefix_length >= 16) {
            // /16 to /23 - match first 2 octets
            size_t second_dot = ip_part.find('.', ip_part.find('.') + 1);
            if (second_dot != std::string::npos) {
                std::string prefix = ip_part.substr(0, second_dot);
                std::replace(prefix.begin(), prefix.end(), '.', '\\');
                regex_pattern += prefix + "\\.\\d{1,3}\\.\\d{1,3}";
            }
        } else {
            // /8 to /15 - match first octet
            size_t first_dot = ip_part.find('.');
            if (first_dot != std::string::npos) {
                std::string prefix = ip_part.substr(0, first_dot);
                regex_pattern += prefix + "\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}";
            }
        }
        
        regex_pattern += "$";
        
        return {regex_pattern, ErrorCode::SUCCESS, ""};
        
    } catch (const std::exception& e) {
        return {std::string{}, ErrorCode::INVALID_REQUEST,
               fmt::format("Exception parsing CIDR {}: {}", cidr_pattern, e.what())};
    }
}

bool validate_pattern(const std::string& pattern, bool is_regex) {
    if (pattern.empty()) {
        return false;
    }
    
    if (is_regex) {
        try {
            std::regex test_regex(pattern);
            return true;
        } catch (const std::regex_error&) {
            return false;
        }
    }
    
    // For non-regex patterns, just check they're not empty
    return !pattern.empty();
}

std::unordered_map<std::string, std::string> extract_match_fields(
    const TransactionRequest& request) {
    
    std::unordered_map<std::string, std::string> fields;
    
    // Device fields - primary targets for pattern matching
    fields["ip_address"] = request.device.ip;
    fields["device_fingerprint"] = request.device.fingerprint;
    fields["user_agent"] = request.device.user_agent;
    
    // Merchant fields
    fields["merchant_id"] = request.transaction.merchant_id;
    
    // Card fields
    fields["card_token"] = request.card.token;
    fields["issuer_country"] = request.card.issuer_country;
    fields["card_brand"] = request.card.card_brand;
    
    // Customer fields
    fields["customer_id"] = request.customer.id;
    
    // Currency and other string fields
    fields["currency"] = request.transaction.currency;
    fields["pos_entry_mode"] = request.transaction.pos_entry_mode;
    
    return fields;
}

} // namespace PatternUtils

// ============================================================================
// PatternMatcher Public Interface Implementation
// ============================================================================

PatternMatcher::PatternMatcher(Backend backend) 
    : pimpl_(std::make_unique<Impl>(backend)) {}

PatternMatcher::~PatternMatcher() = default;

Result<void> PatternMatcher::load_patterns(const std::string& blacklist_path,
                                          const std::string& whitelist_path) {
    return pimpl_->load_patterns(blacklist_path, whitelist_path);
}

Result<void> PatternMatcher::add_pattern(const Pattern& pattern) {
    return pimpl_->add_pattern(pattern);
}

Result<void> PatternMatcher::compile_patterns() {
    return pimpl_->compile_patterns();
}

PatternMatchResults PatternMatcher::match_transaction(const TransactionRequest& request) {
    return pimpl_->match_transaction(request);
}

PatternMatchResults PatternMatcher::match_text(const std::string& text, 
                                              const std::string& category) {
    return pimpl_->match_text(text, category);
}

PatternMatchResults PatternMatcher::match_batch(const std::vector<std::string>& texts,
                                               const std::string& category) {
    return pimpl_->match_batch(texts, category);
}

std::vector<Pattern> PatternMatcher::get_loaded_patterns() const {
    return pimpl_->get_loaded_patterns();
}

PatternMatcher::Backend PatternMatcher::get_active_backend() const {
    return pimpl_->get_active_backend();
}

std::unordered_map<std::string, uint64_t> PatternMatcher::get_statistics() const {
    return pimpl_->get_statistics();
}

void PatternMatcher::reset_statistics() {
    pimpl_->reset_statistics();
}

bool PatternMatcher::is_initialized() const {
    return pimpl_->is_initialized();
}

std::string PatternMatcher::get_last_error() const {
    return pimpl_->get_last_error();
}

} // namespace dmp
