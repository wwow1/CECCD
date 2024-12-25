#ifndef CENTER_SERVER_H
#define CENTER_SERVER_H

#include <grpcpp/grpcpp.h>
#include "common.h"
#include <grpcpp/server_builder.h>
#include <iostream>
#include <mutex>
#include <map>
#include <vector>
#include <chrono>
#include <thread>
#include "cloud_edge_cache.grpc.pb.h"
#include <unordered_map>
#include <vector>
#include <string>
#include <queue>
#include <optional>
#include <algorithm>

class CenterServer final : public cloud_edge_cache::EdgeToCenter::Service,
                           public cloud_edge_cache::CenterToEdge::Service {
public:
    // 简化 BlockStats 结构
    using NodeStats = std::pair<int, double>;  // first: access_count, second: total_selectivity
    using BlockStats = std::unordered_map<std::string, NodeStats>;  // node_address -> stats

    CenterServer();

    // Implementation of ReportStatistics API
    grpc::Status ReportStatistics(grpc::ServerContext* context,
                                  const cloud_edge_cache::StatisticsReport* request,
                                  cloud_edge_cache::Empty* response) override;

    void Start(const std::string& server_address);

    // 新增：schema 管理方法
    void updateSchema(const std::string& stream_id, const Common::StreamMeta& meta);
    Common::StreamMeta getStreamMeta(const std::string& stream_id);
    Common::StreamMeta getStreamMeta(const uint32_t unique_id);
private:

    struct PredictionStats {
        std::chrono::system_clock::time_point timestamp;
        std::unordered_map<std::string, double> node_access_frequencies;
        std::unordered_map<std::string, double> node_selectivities;
        std::unordered_map<std::string, double> qccv_values;  // 新增：每个节点的 QCCV 值
        std::string best_cache_node;  // 新增：QCCV 最高的节点
    };

    std::unordered_map<std::string, int> access_count_; // Tracks access counts per key
    uint64_t cache_period_;
    uint64_t block_size_;
    std::vector<std::string> replacement_keys_;         // Keys for cache replacement
    std::string center_addr_;

    std::mutex stats_mutex_;
    std::chrono::milliseconds prediction_period_;
    std::chrono::system_clock::time_point current_period_start_;
    
    // 添加这些成员变量
    std::unordered_map<std::string, BlockStats> current_period_stats_;           // 当前周期的统计数据
    std::unordered_map<std::string, std::vector<BlockStats>> historical_stats_;  // 历史统计数据
    std::unordered_map<std::string, PredictionStats> predictions_;               // 预测结果
    std::vector<std::string> edge_server_addresses_;                             // 边缘服务器地址列表

    // 新增：存储网络带宽和延迟的矩阵
    struct NetworkMetrics {
        double bandwidth;  // MB/s
        double latency;    // seconds
    };
    std::unordered_map<std::string, std::unordered_map<std::string, NetworkMetrics>> network_metrics_;
    
    // 新增：初始化网络度量的方法
    void initializeNetworkMetrics();
    void measureNetworkMetrics(const std::string& from_node, const std::string& to_node);

    double getNetworkBandwidth(const std::string& from_node, const std::string& to_node);
    double getNetworkLatency(const std::string& from_node, const std::string& to_node);

    // 新增：缓存替换队列类
    class CacheReplacementQueue {
    private:
        // 优先队列元素类型定义
        using QueueElement = std::tuple<double, std::string, std::string>; // <QCCV值, 数据块ID, 节点ID>
        
        // 优先队列，按QCCV值降序排列
        std::priority_queue<
            QueueElement,
            std::vector<QueueElement>,
            std::less<QueueElement>
        > priority_queue_;
        
        // 每个数据块的候选缓存位置
        std::unordered_map<
            std::string, 
            std::vector<std::pair<std::string, double>>  // <节点ID, QCCV值>
        > block_candidates_;

    public:
        // 添加新的私有方法
        void addNextBestCandidate(const std::string& block_key);

        // 添加数据块的QCCV值及其对应的边缘节点
        void addBlockQCCV(const std::string& block_key, 
                         const std::string& node, 
                         double qccv);
        
        // 构建优先队列
        void buildPriorityQueue();
        
        // 获取下一个最优的缓存放置选择
        std::optional<std::pair<std::string, std::string>> getTopPlacementChoice();
        
        // 清空队列
        void clear() {
            while (!priority_queue_.empty()) {
                priority_queue_.pop();
            }
            block_candidates_.clear();
        }
    };

    // 新增：节点容量管理结构
    struct NodeCapacity {
        size_t total_capacity;     // 总容量
        size_t used_capacity;      // 已使用容量
        std::unordered_map<std::string, size_t> cached_blocks; // 已缓存的数据块及其大小
    };

    // 新增：成员变量
    CacheReplacementQueue replacement_queue_;
    std::unordered_map<std::string, NodeCapacity> node_capacities_;

    // 新增：全局 schema 管理
    std::mutex schema_mutex_;
    std::unordered_map<std::string, Common::StreamMeta> schema_;
    
    // 新增：基于 unique_id 的 schema 索引
    std::unordered_map<uint32_t, std::string> unique_id_to_datastream_id_;

    // 新增：当前周期的数据块分配记录
    // node_addr -> set<block_id>
    std::unordered_map<std::string, std::unordered_set<std::string>> node_block_allocation_map_;

    using WeightedStats = std::unordered_map<std::string, std::pair<double, double>>;

    WeightedStats calculateWeightedStats(const std::vector<BlockStats>& history);
    
    std::unordered_map<std::string, double> calculateNodeQCCVs(
        const std::string& block_key, 
        const WeightedStats& weighted_stats);
    
    std::pair<
        std::unordered_map<std::string, std::vector<std::string>>,
        std::unordered_map<std::string, std::vector<std::string>>
    > calculateIncrementalChanges(
        const std::unordered_map<std::string, std::unordered_set<std::string>>& new_allocation_plan);
    
    void executeNodeCacheUpdate(
        const std::string& edge_server_address,
        const std::vector<std::string>& blocks_to_add,
        const std::vector<std::string>& blocks_to_remove);

    void executeCacheReplacement(
        const std::unordered_map<std::string, std::unordered_set<std::string>>& new_allocation_plan);
    
    std::unordered_map<std::string, std::unordered_set<std::string>> generateAllocationPlan();

    void calculateQCCVs();
    void cacheReplacementLoop();
    void updateAccessHistory(const std::unordered_map<std::string, BlockStats>& period_stats);
    std::unordered_map<std::string, BlockStats> collectPeriodStats();

    bool checkNodeCapacity(const std::string& node);
    void updateNodeCapacity(const std::string& node);
};

#endif // CENTER_SERVER_H
