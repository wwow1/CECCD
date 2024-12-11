#ifndef CENTER_SERVER_H
#define CENTER_SERVER_H

#include <grpcpp/grpcpp.h>
#include "cloud_edge_cache.grpc.pb.h"
#include <unordered_map>
#include <vector>
#include <string>

class CenterServer final : public cloud_edge_cache::EdgeToCenter::Service,
                           public cloud_edge_cache::CenterToEdge::Service {
public:
    CenterServer();

    // Implementation of ReportStatistics API
    grpc::Status ReportStatistics(grpc::ServerContext* context,
                                  const cloud_edge_cache::StatisticsReport* request,
                                  cloud_edge_cache::Empty* response) override;

    void ReplaceCache(const std::vector<std::string>& keys);

    void Start(const std::string& server_address);

private:
    std::unordered_map<std::string, int> access_count_; // Tracks access counts per key
    std::vector<std::string> replacement_keys_;         // Keys for cache replacement
};

#endif // CENTER_SERVER_H
