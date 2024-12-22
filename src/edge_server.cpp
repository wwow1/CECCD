#include "edge_server.h"

EdgeServer::EdgeServer() {
    // 加载配置
    auto& config = ConfigManager::getInstance();
    config.loadConfig("config/cluster_config.json");
    
    // 获取中心节点地址并测量延迟
    center_addr_ = config.getCenterAddress();
    int64_t center_latency = measureLatency(center_addr_);
    std::cout << "Center node latency: " << center_latency << "ms" << std::endl;
    
    // 初始化邻居节点地址，只保留延迟小于中心节点的邻居
    for (const auto& node : config.getNodes()) {
        int64_t node_latency = measureLatency(node.address);
        std::cout << "Node " << node.address << " latency: " << node_latency << "ms" << std::endl;
        
        if (node_latency < center_latency) {
            neighbor_addrs_.insert(node.address);
            std::cout << "Added " << node.address << " as neighbor" << std::endl;
        }
    }
    
    // 初始化缓存索引
    cache_index_ = std::make_unique<EdgeCacheIndex>();
}

// 新增：测量延迟的辅助函数
//这个实现使用 gRPC 通道建立连接来测量延迟，这比传统的 ICMP ping 
//更能反映实际的应用层延迟。每个节点都会进行3次测量并取平均值，以获得更稳定的结果。
int64_t EdgeServer::measureLatency(const std::string& address) {
    const int PING_COUNT = 3;  // 进行3次ping取平均值
    int64_t total_latency = 0;
    
    for (int i = 0; i < PING_COUNT; i++) {
        auto start = std::chrono::high_resolution_clock::now();
        
        // 创建临时gRPC通道并尝试建立连接
        auto channel = grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
        auto stub = NodeQueryService::NewStub(channel);
        
        // 设置超时时间为1秒
        auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(1);
        channel->WaitForConnected(deadline);
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        total_latency += duration.count();
    }

    return total_latency / PING_COUNT;  // 返回平均延迟
}

// -------------------------------------------------------------------------------------

grpc::Status EdgeServer::Query(grpc::ServerContext* context,
                               const cloud_edge_cache::QueryRequest* request,
                               cloud_edge_cache::QueryResponse* response) {
    std::string sql_query = request->sql_query();
    
    // Parse SQL
    hsql::SQLParserResult result;
    hsql::SQLParser::parse(sql_query, &result);
    
    int64_t start_timestamp = std::numeric_limits<int64_t>::min();
    int64_t end_timestamp = std::numeric_limits<int64_t>::max();
    std::string table_name;

    if (result.isValid()) {
        const hsql::SQLStatement* stmt = result.getStatement(0);
        if (stmt->type() == hsql::kStmtSelect) {
            const hsql::SelectStatement* select = (const hsql::SelectStatement*) stmt;
            // Get table name from FROM clause
            if (select->fromTable) {
                table_name = select->fromTable->getName();
            }
            // Parse timestamp range from WHERE clause
            if (select->whereClause != nullptr) {
                parseWhereClause(select->whereClause, start_timestamp, end_timestamp);
            }
        }
    }

    std::cout << "Query for table: " << table_name 
              << ", timestamp range: [" << start_timestamp << ", " << end_timestamp << "]" << std::endl;

    // Query the cache index to find nodes with relevant data
    auto node_blocks = cache_index_->queryMainIndex(table_name, start_timestamp, end_timestamp);
    
    // Format response with nodes that have the data
    std::set<std::string> nodes_with_data;
    for (const auto& [block_id, node_id] : node_blocks) {
        nodes_with_data.insert(node_id);
    }
    
    // TODO(zhengfuyu): 需要根据node_blocks来获取每个node的block信息,目前先略过
    // std::map<std::string, std::vector<BlockInfo>> node_to_blocks;
    // // Group blocks by node
    // for (const auto& [block_id, node_id] : node_blocks) {
    //     node_to_blocks[node_id].push_back(cache_index_->getBlockInfo(block_id));
    // }

    // Create gRPC channel for each node and send queries
    std::vector<std::future<QueryResult>> query_futures;
    for (const auto& node_id : nodes_with_data) {
        // Async query to each node
        query_futures.push_back(std::async(std::launch::async, [this, node_id, sql_query]() {
            auto stub = NodeQueryService::NewStub(grpc::CreateChannel(
                node_id, grpc::InsecureChannelCredentials()));
            
            QueryRequest node_request;
            node_request.set_sql_query(sql_query);
            
            QueryResponse node_response;
            ClientContext context;
            
            auto status = stub->SubQuery(&context, node_request, &node_response);
            return std::make_pair(status, node_response);
        }));
    }

    // Merge results from all nodes
    for (auto& future : query_futures) {
        auto [status, node_response] = future.get();
        if (status.ok()) {
            mergeQueryResults(response, node_response);
        } else {
            std::cerr << "Query failed for node: " << status.error_message() << std::endl;
        }
    }

    return grpc::Status::OK;
}

void EdgeServer::mergeQueryResults(cloud_edge_cache::QueryResponse* final_response, 
                                 const cloud_edge_cache::QueryResponse& node_response) {
    // If this is the first result, copy column metadata
    if (final_response->columns().empty() && node_response.columns_size() > 0) {
        for (const auto& col : node_response.columns()) {
            auto* new_col = final_response->add_columns();
            new_col->CopyFrom(col);
        }
    }

    // Append rows from node_response to final_response
    for (const auto& row : node_response.rows()) {
        auto* new_row = final_response->add_rows();
        new_row->CopyFrom(row);
    }
}

// 辅助函数来解析 WHERE 子句
void EdgeServer::parseWhereClause(const hsql::Expr* expr, 
                                int64_t& start_timestamp, 
                                int64_t& end_timestamp) {
    if (!expr) return;
    
    if (expr->type == hsql::kExprOperator) {
        if (expr->opType == hsql::kOpAnd) {
            // 递归处理 AND 条件的两边
            parseWhereClause(expr->expr, start_timestamp, end_timestamp);
            parseWhereClause(expr->expr2, start_timestamp, end_timestamp);
        } else if (expr->opType == hsql::kOpOr) {
            // OR 条件可能会使时间范围失效，这里选择保持最宽松的范围
            std::cout << "Warning: OR condition detected, using widest possible time range" << std::endl;
            start_timestamp = std::numeric_limits<int64_t>::min();
            end_timestamp = std::numeric_limits<int64_t>::max();
        } else {
            // 处理比较操作符
            if (expr->expr && expr->expr->type == hsql::kExprColumnRef &&
                std::string(expr->expr->name) == "timestamp") {
                if (expr->expr2 && expr->expr2->type == hsql::kExprLiteralInt) {
                    switch (expr->opType) {
                        case hsql::kOpGreaterEq:
                            start_timestamp = std::max(start_timestamp, expr->expr2->ival);
                            break;
                        case hsql::kOpGreater:
                            start_timestamp = std::max(start_timestamp, expr->expr2->ival + 1);
                            break;
                        case hsql::kOpLessEq:
                            end_timestamp = std::min(end_timestamp, expr->expr2->ival);
                            break;
                        case hsql::kOpLess:
                            end_timestamp = std::min(end_timestamp, expr->expr2->ival - 1);
                            break;
                    }
                }
            }
        }
    }
}

// -------------------------------------------------------------------------------------

grpc::Status EdgeServer::UpdateMetadata(grpc::ServerContext* context,
                                        const cloud_edge_cache::UpdateCacheMeta* request,
                                        cloud_edge_cache::Empty* response) {
    // Simulate metadata update
    std::string src_node_addr = request->src_node_addr();
    std::cout << "Updating metadata with the following keys:" << std::endl;
    for (const auto& meta : request->metadata()) {
        if (cache_index_.find(meta.table_name()) == cache_index_.end()) {
            // TODO（zhengfuyu）：将table的schema初始化放进去
            cache_index_[meta.table_name()] = make_unique<EdgeCacheIndex>();
        }
        (cache_index_[meta.table_name()])->updateIndex(src_node_addr, meta);
    }
    return grpc::Status::OK;
}

// -------------------------------------------------------------------------------------

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

// -------------------------------------------------------------------------------------

void EdgeServer::Start(const std::string& server_address) {
    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(this); // Register both services

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    std::cout << "Edge server is running on " << server_address << std::endl;
    server->Wait();
}

// -------------------------------------------------------------------------------------

void EdgeServer::PushMetadataUpdate(const std::vector<>& keys, const std::string& target_server_address) {
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

// -------------------------------------------------------------------------------------

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

// -------------------------------------------------------------------------------------

grpc::Status EdgeServer::SubQuery(grpc::ServerContext* context,
                                const cloud_edge_cache::QueryRequest* request,
                                cloud_edge_cache::QueryResponse* response) {
    std::string sql_query = request->sql_query();
    
    // Parse SQL
    hsql::SQLParserResult result;
    hsql::SQLParser::parse(sql_query, &result);
    
    if (!result.isValid()) {
        response->set_error("Failed to parse SQL query");
        return grpc::Status::OK;
    }

    try {
        auto& config = ConfigManager::getInstance();
        pqxx::connection conn(config.getDatabaseConfig().getConnectionString());
        pqxx::work txn(conn);
        
        // 执行查询
        pqxx::result db_result = txn.exec(sql_query);
        
        // 设置列信息
        for (const auto& column : db_result.columns()) {
            auto* col = response->add_columns();
            col->set_name(column.name());
            col->set_type(column.type());
        }

        // 添加结果行
        for (const auto& db_row : db_result) {
            auto* row = response->add_rows();
            for (size_t i = 0; i < db_row.size(); ++i) {
                row->add_values(db_row[i].c_str());
            }
        }

        txn.commit();
    } catch (const std::exception& e) {
        response->set_error(std::string("Database error: ") + e.what());
    }

    return grpc::Status::OK;
}

// -------------------------------------------------------------------------------------

int main() {
    // Start edge server
    const std::string server_address = "localhost:50052";
    EdgeServer edge_server;
    edge_server.Start(server_address);

    return 0;
}