#include "center_server.h"
#include <grpcpp/server_builder.h>
#include <iostream>

CenterServer::CenterServer() {}

grpc::Status CenterServer::ReportStatistics(grpc::ServerContext* context,
                                            const cloud_edge_cache::StatisticsReport* request,
                                            cloud_edge_cache::Empty* response) {
    std::cout << "Received statistics report:" << std::endl;
    for (const auto& key : request->metadata().keys()) {
        std::cout << "  Key: " << key << std::endl;
        access_count_[key]++; // Increment access count for the key
    }
    return grpc::Status::OK;
}

void CenterServer::ReplaceCache(const std::vector<std::string>& keys) {
    for (const auto& edge_server_address : edge_server_addresses_) {
        auto stub = cachesystem::CacheReplacementService::NewStub(grpc::CreateChannel(
            edge_server_address, grpc::InsecureChannelCredentials()));

        cachesystem::CacheReplacementRequest request;
        for (const auto& key : keys) {
            request.add_keys(key);
        }

        cachesystem::CacheReplacementResponse response;
        grpc::ClientContext context;

        grpc::Status status = stub->ReplaceCache(&context, request, &response);

        if (status.ok()) {
            std::cout << "Successfully replaced cache on " << edge_server_address << std::endl;
        } else {
            std::cerr << "Failed to replace cache on " << edge_server_address
                      << ": " << status.error_message() << std::endl;
        }
    }
}

void CenterServer::Start(const std::string& server_address) {
    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(this); // Register both services

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    std::cout << "Center server is running on " << server_address << std::endl;
    server->Wait();
}

int main() {
    // Start center server
    const std::string server_address = "localhost:50053";
    CenterServer center_server;
    center_server.Start(server_address);

    return 0;
}