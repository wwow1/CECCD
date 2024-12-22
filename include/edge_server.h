#ifndef EDGE_SERVER_H
#define EDGE_SERVER_H

#include <unordered_map>
#include <string>
#include <iostream>
#include <grpcpp/grpcpp.h>
#include <grpcpp/server_builder.h>
#include "cloud_edge_cache.grpc.pb.h"
#include "edgeCacheIndex.h"
#include "common.h"
#include "SQLParser.h"
#include "sql/SQLStatement.h"
#include <pqxx/pqxx>

class EdgeServer final : public cloud_edge_cache::ClientToEdge::Service,
                         public cloud_edge_cache::EdgeToEdge::Service,
                         public cloud_edge_cache::CenterToEdge::Service {
public:
    EdgeServer();

    // Implementation of Query API
    grpc::Status Query(grpc::ServerContext* context,
                       const cloud_edge_cache::QueryRequest* request,
                       cloud_edge_cache::QueryResponse* response) override;

    // Implementation of UpdateMetadata API
    grpc::Status UpdateMetadata(grpc::ServerContext* context,
                                const cloud_edge_cache::UpdateCacheMeta* request,
                                cloud_edge_cache::Empty* response) override;

    grpc::Status ReplaceCache(grpc::ServerContext* context, 
                              const ::cloud_edge_cache::CacheReplacement* request,
                              cloud_edge_cache::Empty* response) override;

    // Implementation of SubQuery API
    grpc::Status SubQuery(grpc::ServerContext* context,
                         const cloud_edge_cache::QueryRequest* request,
                         cloud_edge_cache::QueryResponse* response) override;

    // 客户端调用其他边缘服务器的方
    void PushMetadataUpdate(const std::vector<std::string>& keys, const std::string& target_server_address);

    void ReportStatistics(const std::vector<std::string>& keys);

    void Start(const std::string& server_address);
private:
    void mergeQueryResults(cloud_edge_cache::QueryResponse* final_response, 
                           const cloud_edge_cache::QueryResponse& node_response);

    void parseWhereClause(const hsql::Expr* expr, 
                          int64_t& start_timestamp, 
                          int64_t& end_timestamp);

    int64_t measureLatency(const std::string& address);
    
private:
    std::unique_ptr<EdgeCacheIndex> cache_index_;
    std::set<std::string> neighbor_addrs_; // 直接填ip:port
    std::string center_addr_;
};

#endif // EDGE_SERVER_H
