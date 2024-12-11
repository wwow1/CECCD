#ifndef EDGE_SERVER_H
#define EDGE_SERVER_H

#include <grpcpp/grpcpp.h>
#include "cloud_edge_cache.grpc.pb.h"
#include <unordered_map>
#include <string>

class EdgeServer final : public cloud_edge_cache::ClientToEdge::Service,
                         public cloud_edge_cache::EdgeToEdge::Service {
public:
    EdgeServer();

    // Implementation of Query API
    grpc::Status Query(grpc::ServerContext* context,
                       const cloud_edge_cache::QueryRequest* request,
                       cloud_edge_cache::QueryResponse* response) override;

    // Implementation of UpdateMetadata API
    grpc::Status UpdateMetadata(grpc::ServerContext* context,
                                const cloud_edge_cache::Metadata* request,
                                cloud_edge_cache::Empty* response) override;

    grpc::Status ReplaceCache(grpc::ServerContext* context, 
                              const cloud_edge_cache::Metadata* request,
                              cloud_edge_cache::Empty* response) override;

    // 客户端调用其他边缘服务器的方法
    void PushMetadataUpdate(const std::vector<std::string>& keys, const std::string& target_server_address);

    void ReportStatistics(const std::vector<std::string>& keys);

    void Start(const std::string& server_address);

private:
    std::unordered_map<std::string, std::string> cache_; // Simulated cache
};

#endif // EDGE_SERVER_H
