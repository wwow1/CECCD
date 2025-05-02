#include "config_manager.h"

void ConfigManager::loadConfig(const std::string& config_path) {
    std::ifstream file(config_path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open config file: " + config_path);
    }

    nlohmann::json j;
    file >> j;

    // Load center node address
    center_address_ = j["cluster"]["center_node"]["address"].get<std::string>();

    // Load database config
    db_config_.host = j["database"]["host"].get<std::string>();
    db_config_.port = j["database"]["port"].get<std::string>();
    db_config_.dbname = j["database"]["dbname"].get<std::string>();
    db_config_.user = j["database"]["user"].get<std::string>();
    db_config_.password = j["database"]["password"].get<std::string>();

    // Load index latency threshold
    index_latency_threshold_ms_ = j["cluster"]["index_latency_threshold_ms"].get<int64_t>();
    
    // Load statistics report interval
    statistics_report_interval_s_ = j["cluster"]["statistics_report_interval_s"].get<int64_t>();
    
    // Load prediction period
    prediction_period_s_ = j["cluster"]["prediction_period_s"].get<int64_t>();

    // Load block size
    block_size_mb_ = j["cluster"]["block_size_mb"].get<double>();
    
    // Load edge capacity
    edge_capacity_gb_ = j["cluster"]["edge_capacity_gb"].get<double>();

    // Load bloom filter false positive rate
    bloom_filter_fpr_ = j["cluster"]["bloom_filter_fpr"].get<double>();

    // Load log file path
    log_file_path_ = j["cluster"]["log_file_path"].get<std::string>();

    // Load log levels
    edge_log_level_ = j["cluster"]["edge_log_level"].get<std::string>();
    center_log_level_ = j["cluster"]["center_log_level"].get<std::string>();
} 