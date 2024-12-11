#include "edge_server.h"
#include <grpcpp/server_builder.h>
#include <iostream>

EdgeServer::EdgeServer() {
    // Simulated initial cache (key-value pairs)
    cache_["table1:source1:1660000000"] = "{\"value\": 42}";
    cache_["table2:source2:1660000100"] = "{\"value\": 23}";
}

grpc::Status EdgeServer::Query(grpc::ServerContext* context,
                               const cloud_edge_cache::QueryRequest* request,
                               cloud_edge_cache::QueryResponse* response) {
    std::string sql_query = request->sql_query();
    // TODO(zhengfuyu): 把timestamp提取出来,其它的照旧
    if (cache_.find(sql_query) != cache_.end()) {
        response->set_result(cache_[sql_query]);
    } else {
        response->set_result("No data found for the given query.");
    }
    return grpc::Status::OK;
}

grpc::Status EdgeServer::UpdateMetadata(grpc::ServerContext* context,
                                        const cloud_edge_cache::Metadata* request,
                                        cloud_edge_cache::Empty* response) {
    // Simulate metadata update
    std::cout << "Updating metadata with the following keys:" << std::endl;
    for (const auto& key : request->keys()) {
        std::cout << key << std::endl;
        // Simulate adding or refreshing cache entry
        cache_[key] = "{\"value\": \"updated\"}"; // Dummy updated value
    }
    return grpc::Status::OK;
}

grpc::Status ReplaceCache(grpc::ServerContext* context, 
                          const cloud_edge_cache::Metadata* request,
                          cloud_edge_cache::Empty* response) {
    for (const auto& key : request->keys()) {
        std::cout << "Received cache replacement request for key: " << key << std::endl;
        // 模拟缓存替换逻辑
    }
    response->set_success(true);
    return grpc::Status::OK;
}

void EdgeServer::Start(const std::string& server_address) {
    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(this); // Register both services

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    std::cout << "Edge server is running on " << server_address << std::endl;
    server->Wait();
}

void EdgeServer::PushMetadataUpdate(const std::vector<std::string>& keys, const std::string& target_server_address) {
    // 建立目标边缘服务器的 gRPC 通道
    auto stub = cachesystem::MetadataUpdateService::NewStub(grpc::CreateChannel(
        target_server_address, grpc::InsecureChannelCredentials()));

    // 准备请求
    cachesystem::MetadataUpdateRequest request;
    for (const auto& key : keys) {
        request.add_keys(key);
    }

    // 响应和上下文
    cachesystem::MetadataUpdateResponse response;
    grpc::ClientContext context;

    // 发起请求
    grpc::Status status = stub->UpdateMetadata(&context, request, &response);

    if (status.ok()) {
        std::cout << "Successfully pushed metadata update to " << target_server_address << std::endl;
    } else {
        std::cerr << "Failed to push metadata update to " << target_server_address 
                  << ": " << status.error_message() << std::endl;
    }
}

void EdgeServer::ReportStatistics(const std::vector<std::string>& keys) {
    auto stub = cachesystem::ReportStatisticsService::NewStub(grpc::CreateChannel(
        center_server_address_, grpc::InsecureChannelCredentials()));

    cachesystem::StatisticsRequest request;
    for (const auto& key : keys) {
        request.add_keys(key);
    }

    cachesystem::StatisticsResponse response;
    grpc::ClientContext context;

    grpc::Status status = stub->ReportStatistics(&context, request, &response);

    if (status.ok()) {
        std::cout << "Successfully reported statistics to center server" << std::endl;
    } else {
        std::cerr << "Failed to report statistics to center server: " << status.error_message() << std::endl;
    }
}

int main() {
    // Start edge server
    const std::string server_address = "localhost:50052";
    EdgeServer edge_server;
    edge_server.Start(server_address);

    return 0;
}
