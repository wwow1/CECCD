#ifndef CLIENT_H
#define CLIENT_H

#include <grpcpp/grpcpp.h>
#include "cloud_edge_cache.grpc.pb.h"
#include <string>

class Client {
public:
    Client(const std::string& server_address);
    std::string Query(const std::string& sql_query);

private:
    std::unique_ptr<cloud_edge_cache::ClientToEdge::Stub> stub_;
};

#endif // CLIENT_H
