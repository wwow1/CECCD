#include "config_manager.h"

void ConfigManager::loadConfig(const std::string& config_path) {
    std::ifstream file(config_path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open config file: " + config_path);
    }

    nlohmann::json j;
    file >> j;

    // Load nodes
    for (const auto& node : j["cluster"]["nodes"]) {
        nodes.push_back({
            node["id"].get<std::string>(),
            node["address"].get<std::string>()
        });
    }

    // Load center node address
    center_address = j["cluster"]["center_node"]["address"].get<std::string>();

    // Load database config
    db_config.host = j["database"]["host"].get<std::string>();
    db_config.port = j["database"]["port"].get<std::string>();
    db_config.dbname = j["database"]["dbname"].get<std::string>();
    db_config.user = j["database"]["user"].get<std::string>();
    db_config.password = j["database"]["password"].get<std::string>();
} 