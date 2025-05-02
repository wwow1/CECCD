#ifndef CLIENT_H
#define CLIENT_H

#include <string>
#include <vector>
#include "cloud_edge_cache.grpc.pb.h"

class Client {
public:
    explicit Client(const std::string& center_server_address);
    void BindToEdgeServer(const std::string& edge_server_address);
    std::string Query(const std::string& sql_query);
    std::vector<std::string> GetEdgeNodes();

private:
    std::unique_ptr<cloud_edge_cache::ClientToEdge::Stub> edge_stub_;
    std::unique_ptr<cloud_edge_cache::ClientToCenter::Stub> center_stub_;
};

#endif // CLIENT_H
