#include <iostream>
#include <string>
#include "edge_server.h"
  
int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <ip_address> <port> <config_path>" << std::endl;
        return 1;
    }

    const std::string ip_address = argv[1];
    const std::string port = argv[2];
    const std::string config_path = argv[3];
    const std::string server_address = ip_address + ":" + port;
    
    try {
        auto& config = ConfigManager::getInstance();
        config.loadConfig(config_path);
        
        EdgeServer edge_server;
        edge_server.Start(server_address);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}