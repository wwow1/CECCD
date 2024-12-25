#ifndef EDGE_SERVER_H
#define EDGE_SERVER_H

#include <unordered_map>
#include <string>
#include <iostream>
#include <grpcpp/grpcpp.h>
#include <grpcpp/server_builder.h>
#include "cloud_edge_cache.grpc.pb.h"
#include "edge_cache_index.h"
#include "common.h"
#include "SQLParser.h"
#include "sql/SQLStatement.h"
#include <pqxx/pqxx>
#include <thread>
#include <atomic>
#include <mutex>
#include <optional>

class EdgeServer final : public cloud_edge_cache::ClientToEdge::Service,
                         public cloud_edge_cache::EdgeToEdge::Service,
                         public cloud_edge_cache::CenterToEdge::Service {
public:
    EdgeServer();
    ~EdgeServer();

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

    void PushMetadataUpdate(const std::vector<std::string>& keys, const std::string& target_server_address);

    void ReportStatistics(const std::vector<std::string>& keys);

    void Start(const std::string& server_address);

    cloud_edge_cache::SubQueryResponse SubQuery(const std::string& node_id, 
                                                const std::string& sql_query, 
                                                const uint32_t block_id,
                                                const uint32_t stream_unique_id);

    // 添加新的方法用于收集需要上报的统计信息
    void addStatsToReport(const uint32_t stream_unique_id, const uint32_t block_id, const double query_selectivity);

private:
    void parseWhereClause(const hsql::Expr* expr, 
                          int64_t& start_timestamp, 
                          int64_t& end_timestamp);

    int64_t measureLatency(const std::string& address);

    std::optional<Common::StreamMeta> getStreamMeta(const std::string& datastream_id) {
        typename tbb::concurrent_hash_map<std::string, Common::StreamMeta>::const_accessor accessor;
        if (schema_.find(accessor, datastream_id)) {
            return accessor->second;
        }
        return std::nullopt;
    }

    std::string addBlockConditions(const std::string& original_sql, 
                                   const std::string& datastream_id,
                                   const uint32_t block_id);

    uint32_t getBlockId(uint64_t block_start_time, std::string datastream_id) {
        auto& source_schema = getStreamMeta(datastream_id);
        uint32_t blockId = (block_start_time - source_schema.start_time_) / source_schema.time_range_;
        return blockId;
    }
    
    std::pair<uint32_t, uint32_t> getBlockRange(const std::string& datastream_id, 
                                                int64_t start_timestamp, 
                                                int64_t end_timestamp) {
        uint32_t start_block = getBlockId(start_timestamp, datastream_id);
        uint32_t end_block = getBlockId(end_timestamp, datastream_id);
        return {start_block, end_block};
    }

    // 统计信息上报循环
    void statsReportLoop();

    cloud_edge_cache::SubQueryResponse executeSubQuery(const std::string& node_id, 
                                                     const std::string& sql_query, 
                                                     const uint32_t block_id,
                                                     const uint32_t stream_unique_id);

private:
    std::unique_ptr<EdgeCacheIndex> cache_index_;
    tbb::concurrent_hash_map<std::string, Common::StreamMeta> schema_;

    std::set<std::string> neighbor_addrs_; // 直接填ip:port
    std::string center_addr_;
    std::string server_address_;

    // 统计信息相关成员
    std::thread stats_report_thread_;
    std::atomic<bool> should_stop_{false};
    std::mutex stats_mutex_;
    std::vector<cloud_edge_cache::BlockAccessInfo> pending_report_access_info_;

    // 定义查询任务结构体
    struct QueryTask {
        std::string node_id;
        std::string sql_query;
        uint32_t block_id;
        uint32_t stream_uniqueId;

        QueryTask(std::string node, std::string sql, uint32_t block, uint32_t stream_id)
            : node_id(std::move(node))
            , sql_query(std::move(sql))
            , block_id(block)
            , stream_uniqueId(stream_id) {}
    };

    // 新增的辅助函数声明
    std::tuple<std::string, int64_t, int64_t> parseSQLQuery(const std::string& sql_query);
    
    std::vector<std::pair<QueryTask, std::future<cloud_edge_cache::SubQueryResponse>>> 
    createQueryTasks(const std::string& sql_query, 
                    const std::string& table_name,
                    uint32_t start_block, 
                    uint32_t end_block,
                    uint32_t stream_uniqueId);
    
    void mergeQueryResults(cloud_edge_cache::QueryResponse* response,
                          const std::vector<cloud_edge_cache::QueryResponse>& all_results);

    std::vector<cloud_edge_cache::QueryResponse> processQueryResults(
        std::vector<std::pair<QueryTask, std::future<cloud_edge_cache::SubQueryResponse>>>& query_tasks);

    // ReplaceCache 相关的辅助函数
    void updateSchemaInfo(const cloud_edge_cache::CacheReplacement* request);
    
    void ensureTableExists(const std::string& table_name,
                          pqxx::connection& local_conn,
                          pqxx::connection& center_conn);
    
    std::string buildCreateTableQuery(const std::string& table_name,
                                    const pqxx::result& schema_info);
    
    void addDataBlock(const std::string& table_name,
                     uint64_t block_start_time,
                     uint64_t block_end_time,
                     pqxx::connection& local_conn,
                     pqxx::connection& center_conn);
    
    std::string buildInsertQuery(const std::string& table_name,
                                const pqxx::result& rows);
    
    void removeDataBlock(const std::string& table_name,
                        uint64_t block_start_time,
                        uint64_t block_end_time,
                        pqxx::connection& local_conn);
};

#endif // EDGE_SERVER_H
