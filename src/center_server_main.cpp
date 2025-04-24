#include <iostream>
#include <string>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include "center_server.h"

std::string get_local_ip() {
    struct ifaddrs *ifaddr, *ifa;
    getifaddrs(&ifaddr);
    
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_INET)
            continue;
            
        struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
        std::string ip = inet_ntoa(sa->sin_addr);
        if (ip != "127.0.0.1") {
            freeifaddrs(ifaddr);
            return ip;
        }
    }
    freeifaddrs(ifaddr);
    return "127.0.0.1"; // fallback
}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <port> <config_path>" << std::endl;
        return 1;
    }
    const std::string port = argv[1];
    const std::string config_path = argv[2];
    const std::string ip_address = get_local_ip();
    const std::string center_address = ip_address + ":" + port;

    try {
        auto& config = ConfigManager::getInstance();
        config.loadConfig(config_path);
        
        CenterServer center_server;
        center_server.Start(center_address);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}