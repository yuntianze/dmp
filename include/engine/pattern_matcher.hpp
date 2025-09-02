/**
 * @file pattern_matcher.hpp
 * @brief High-performance pattern matching interface for DMP risk control system
 * @author Stan Jiang
 * @date 2025-08-28
 */
#pragma once

#include "common/types.hpp"
#include "core/transaction.hpp"
#include <string>
#include <vector>
#include <memory>
#include <unordered_set>
#include <regex>
#include <functional>

namespace dmp {

/**
 * @brief Pattern match result for a single pattern
 * 
 * Contains information about a successful pattern match
 * including the pattern ID and matched text position.
 */
struct PatternMatch {
    uint32_t pattern_id;          // Unique pattern identifier
    std::string pattern_name;     // Human-readable pattern name
    std::string matched_text;     // The text that matched the pattern
    size_t start_offset;          // Start position in the input text
    size_t end_offset;            // End position in the input text
    std::string category;         // Pattern category (blacklist/whitelist)
    
    PatternMatch() : pattern_id(0), start_offset(0), end_offset(0) {}
    
    PatternMatch(uint32_t id, const std::string& name, const std::string& text,
                size_t start, size_t end, const std::string& cat)
        : pattern_id(id), pattern_name(name), matched_text(text),
          start_offset(start), end_offset(end), category(cat) {}
};

/**
 * @brief Pattern matching configuration
 * 
 * Defines patterns to be compiled and used for matching.
 * Supports both exact string matches and regex patterns.
 */
struct Pattern {
    uint32_t id;                  // Unique pattern ID
    std::string name;             // Human-readable name
    std::string pattern;          // Pattern string (regex or exact)
    std::string category;         // Category (ip_blacklist, merchant_blacklist, etc.)
    bool is_regex;                // Whether pattern is a regex
    bool case_sensitive;          // Case sensitivity flag
    uint32_t priority;            // Pattern priority (higher = more important)
    
    Pattern() : id(0), is_regex(false), case_sensitive(true), priority(0) {}
    
    Pattern(uint32_t pattern_id, const std::string& pattern_name, 
           const std::string& pattern_str, const std::string& cat)
        : id(pattern_id), name(pattern_name), pattern(pattern_str), category(cat),
          is_regex(false), case_sensitive(true), priority(0) {}
};

/**
 * @brief Pattern matching results for a complete evaluation
 * 
 * Aggregates all pattern matches found during evaluation
 * with performance metrics and categorized results.
 */
struct PatternMatchResults {
    std::vector<PatternMatch> matches;        // All pattern matches found
    std::vector<PatternMatch> blacklist_matches;  // Blacklist matches only
    std::vector<PatternMatch> whitelist_matches;  // Whitelist matches only
    double evaluation_time_us;                // Total evaluation time
    size_t patterns_checked;                  // Number of patterns evaluated
    size_t texts_processed;                   // Number of input texts processed
    
    PatternMatchResults() : evaluation_time_us(0.0), patterns_checked(0), texts_processed(0) {}
    
    /**
     * @brief Check if any blacklist patterns matched
     * @return true if blacklist matches were found
     */
    bool has_blacklist_matches() const {
        return !blacklist_matches.empty();
    }
    
    /**
     * @brief Check if any whitelist patterns matched
     * @return true if whitelist matches were found
     */
    bool has_whitelist_matches() const {
        return !whitelist_matches.empty();
    }
    
    /**
     * @brief Get total number of matches
     * @return Total match count
     */
    size_t total_matches() const {
        return matches.size();
    }
    
    /**
     * @brief Calculate match score based on priority
     * @return Weighted match score
     */
    float calculate_match_score() const {
        float score = 0.0f;
        for (const auto& match : matches) {
            // Blacklist matches add positive score (risk)
            if (match.category.find("blacklist") != std::string::npos) {
                score += 10.0f; // Base blacklist score
            }
            // Whitelist matches reduce score (trust)
            else if (match.category.find("whitelist") != std::string::npos) {
                score -= 5.0f; // Trust reduction
            }
        }
        return std::max(0.0f, score); // Ensure non-negative
    }
};

/**
 * @brief Pattern matching engine with multiple backend support
 * 
 * Provides high-performance pattern matching using Hyperscan when available,
 * with fallback to standard library regex for compatibility.
 * Optimized for fraud detection patterns (IP addresses, merchant IDs, etc.).
 */
class PatternMatcher {
public:
    /**
     * @brief Pattern matcher backend types
     */
    enum class Backend {
        AUTO,        // Automatically select best available backend
        HYPERSCAN,   // Intel Hyperscan (high performance)
        STD_REGEX,   // Standard library regex (fallback)
        VECTORSCAN   // Vectorscan (ARM-compatible fork of Hyperscan)
    };
    
    /**
     * @brief Constructor
     * @param backend Preferred backend type
     */
    explicit PatternMatcher(Backend backend = Backend::AUTO);
    
    /**
     * @brief Destructor - ensures clean resource cleanup
     */
    ~PatternMatcher();
    
    // Non-copyable and non-movable for resource safety
    PatternMatcher(const PatternMatcher&) = delete;
    PatternMatcher& operator=(const PatternMatcher&) = delete;
    PatternMatcher(PatternMatcher&&) = delete;
    PatternMatcher& operator=(PatternMatcher&&) = delete;
    
    /**
     * @brief Load patterns from configuration files
     * @param blacklist_path Path to blacklist patterns file
     * @param whitelist_path Path to whitelist patterns file
     * @return Result indicating success or error details
     * 
     * Loads and compiles patterns from both blacklist and whitelist files.
     * Patterns are compiled into an optimized database for fast matching.
     */
    Result<void> load_patterns(const std::string& blacklist_path,
                              const std::string& whitelist_path);
    
    /**
     * @brief Add custom pattern programmatically
     * @param pattern Pattern definition to add
     * @return Result indicating success or error details
     */
    Result<void> add_pattern(const Pattern& pattern);
    
    /**
     * @brief Compile all loaded patterns into optimized database
     * @return Result indicating compilation success or errors
     * 
     * Must be called after loading patterns and before matching.
     * This is an expensive operation that should be done once.
     */
    Result<void> compile_patterns();
    
    /**
     * @brief Match patterns against transaction request data
     * @param request Transaction request containing text fields to match
     * @return Pattern match results with all matches and performance metrics
     * 
     * This is the main matching function called for each transaction.
     * Performance target: < 2ms for 100+ patterns against typical transaction.
     * Thread-safe: Yes, uses read-only compiled pattern database.
     */
    PatternMatchResults match_transaction(const TransactionRequest& request);
    
    /**
     * @brief Match patterns against single text input
     * @param text Input text to match against
     * @param category Optional category filter (e.g., "ip_blacklist")
     * @return Pattern match results for the single text
     * 
     * Lower-level matching function for specific text inputs.
     * Useful for testing individual fields or custom text.
     */
    PatternMatchResults match_text(const std::string& text, 
                                  const std::string& category = "");
    
    /**
     * @brief Batch match patterns against multiple texts
     * @param texts Vector of input texts to match
     * @param category Optional category filter
     * @return Aggregated pattern match results
     * 
     * Optimized for batch processing multiple texts simultaneously.
     * Can leverage Hyperscan's batch mode for better performance.
     */
    PatternMatchResults match_batch(const std::vector<std::string>& texts,
                                   const std::string& category = "");
    
    /**
     * @brief Get information about loaded patterns
     * @return Vector of all loaded pattern definitions
     */
    std::vector<Pattern> get_loaded_patterns() const;
    
    /**
     * @brief Get backend information
     * @return Currently active backend type
     */
    Backend get_active_backend() const;
    
    /**
     * @brief Get pattern statistics
     * @return Pattern usage and performance statistics
     */
    std::unordered_map<std::string, uint64_t> get_statistics() const;
    
    /**
     * @brief Reset pattern usage statistics
     */
    void reset_statistics();
    
    /**
     * @brief Check if pattern matcher is properly initialized
     * @return true if loaded and compiled for matching
     */
    bool is_initialized() const;
    
    /**
     * @brief Get last error message if initialization failed
     * @return Error message or empty string if no error
     */
    std::string get_last_error() const;

private:
    // Implementation details hidden using PIMPL pattern
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

/**
 * @brief Pattern file parser utility functions
 * 
 * Utilities for parsing pattern files in various formats
 * and converting them to internal pattern representations.
 */
namespace PatternUtils {

/**
 * @brief Parse patterns from text file
 * @param file_path Path to pattern file
 * @param category Category to assign to all patterns in file
 * @return Vector of parsed patterns or error
 * 
 * Supports common pattern file formats:
 * - One pattern per line
 * - Comments starting with #
 * - CIDR notation for IP ranges
 * - Wildcard patterns with *
 */
Result<std::vector<Pattern>> parse_pattern_file(const std::string& file_path,
                                               const std::string& category);

/**
 * @brief Convert wildcard pattern to regex
 * @param wildcard_pattern Pattern with * wildcards
 * @return Equivalent regex pattern
 * 
 * Converts simple wildcard patterns (e.g., "MERCH_*") to
 * equivalent regex patterns for matching engines.
 */
std::string wildcard_to_regex(const std::string& wildcard_pattern);

/**
 * @brief Convert CIDR notation to regex pattern
 * @param cidr_pattern CIDR pattern (e.g., "192.168.1.0/24")
 * @return Regex pattern matching IP addresses in range
 * 
 * Converts CIDR notation to regex patterns that match
 * individual IP addresses within the specified range.
 */
Result<std::string> cidr_to_regex(const std::string& cidr_pattern);

/**
 * @brief Validate pattern syntax
 * @param pattern Pattern string to validate
 * @param is_regex Whether pattern should be treated as regex
 * @return true if pattern is syntactically valid
 */
bool validate_pattern(const std::string& pattern, bool is_regex);

/**
 * @brief Extract text fields from transaction for pattern matching
 * @param request Transaction request to extract from
 * @return Map of field names to text values for matching
 * 
 * Extracts all relevant text fields that should be checked
 * against patterns: IP address, merchant ID, device fingerprint, etc.
 */
std::unordered_map<std::string, std::string> extract_match_fields(
    const TransactionRequest& request);

} // namespace PatternUtils

} // namespace dmp
