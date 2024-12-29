#include <iostream>
#include <string>
#include "center_server.h"

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <ip_address> <port>" << std::endl;
        return 1;
    }

    const std::string ip_address = argv[1];
    const std::string port = argv[2];
    const std::string server_address = ip_address + ":" + port;
    
    CenterServer center_server;
    center_server.Start(server_address);

    return 0;
}