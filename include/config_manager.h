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
};

class ConfigManager {
public:
    static ConfigManager& getInstance() {
        static ConfigManager instance;
        return instance;
    }

    void loadConfig(const std::string& config_path);
    const std::vector<NodeInfo>& getNodes() const { return nodes; }
    const std::string& getCenterAddress() const { return center_address; }
    const DatabaseConfig& getDatabaseConfig() const { return db_config; }

private:
    ConfigManager() = default;
    std::vector<NodeInfo> nodes;
    std::string center_address;
    DatabaseConfig db_config;
};

#endif // CONFIG_MANAGER_H 