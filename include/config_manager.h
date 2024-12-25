#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <fstream>

struct DatabaseConfig {
    std::string host;
    std::string port;
    std::string dbname;
    std::string user;
    std::string password;

    std::string getConnectionString() const {
        return "postgresql://" + user + ":" + password + "@" 
               + host + ":" + port + "/" + dbname;
    }
};

struct NodeInfo {
    std::string id;
    std::string address;
    double capacity_gb;
};

class ConfigManager {
public:
    static ConfigManager& getInstance() {
        static ConfigManager instance;
        return instance;
    }

    void loadConfig(const std::string& config_path);
    const std::vector<NodeInfo>& getNodes() const { return nodes_; }
    const std::string& getCenterAddress() const { return center_address_; }
    const DatabaseConfig& getDatabaseConfig() const { return db_config_; }
    int64_t getIndexLatencyThresholdMs() const { return index_latency_threshold_ms_; }
    int64_t getStatisticsReportIntervalMs() const { return statistics_report_interval_ms_; }
    int64_t getPredictionPeriodMs() const { return prediction_period_ms_; }

    DatabaseConfig getNodeDatabaseConfig(const std::string& node_address) const {
        DatabaseConfig node_db = db_config_;
        
        size_t colon_pos = node_address.find(':');
        if (colon_pos != std::string::npos) {
            node_db.host = node_address.substr(0, colon_pos);
        }
        
        return node_db;
    }

    size_t getBlockSizeMB() const { return block_size_mb_; }
    double getNodeCapacityGB(const std::string& node_id) const {
        for (const auto& node : nodes_) {
            if (node.id == node_id) return node.capacity_gb;
        }
        return 1.0;
    }

private:
    ConfigManager() = default;
    std::vector<NodeInfo> nodes_;
    std::string center_address_;
    DatabaseConfig db_config_;
    int64_t index_latency_threshold_ms_;
    int64_t statistics_report_interval_ms_;
    int64_t prediction_period_ms_;
    size_t block_size_mb_;
};

#endif // CONFIG_MANAGER_H 