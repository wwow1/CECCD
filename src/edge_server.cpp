#include "edge_server.h"
EdgeServer::EdgeServer() {
    // 加载配置
    auto& config = ConfigManager::getInstance();
    config.loadConfig("config/cluster_config.json");
    
    // 获取中心节点地址并测量延迟
    center_addr_ = config.getCenterAddress();
    int64_t center_latency = measureLatency(center_addr_);
    std::cout << "Center node latency: " << center_latency << "ms" << std::endl;
    
    // 初始化缓存索引
    cache_index_ = std::make_unique<EdgeCacheIndex>();
    cache_index_->setLatencyThreshold(config.getIndexLatencyThresholdMs());

    // 初始化邻居节点地址，只保留延迟小于中心节点的邻居
    for (const auto& node : config.getNodes()) {
        int64_t node_latency = measureLatency(node.address);
        std::cout << "Node " << node.address << " latency: " << node_latency << "ms" << std::endl;
        
        if (node_latency < center_latency) {
            neighbor_addrs_.insert(node.address);
            // 将节点延迟信息传递给缓存索引
            cache_index_->setNodeLatency(node.address, node_latency);
            std::cout << "Added " << node.address << " as neighbor" << std::endl;
        }
    }

    // 启动统计信息上报线程
    stats_report_thread_ = std::thread(&EdgeServer::statsReportLoop, this);
}

EdgeServer::~EdgeServer() {
    // 停止统计信息上报线程
    should_stop_ = true;
    if (stats_report_thread_.joinable()) {
        stats_report_thread_.join();
    }
}

void EdgeServer::statsReportLoop() {
    auto& config = ConfigManager::getInstance();
    int64_t report_interval = config.getStatisticsReportIntervalMs();
    
    while (!should_stop_) {
        // 收集需要上报的统计信息
        std::vector<cloud_edge_cache::BlockAccessInfo> info_to_report;
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            info_to_report = std::move(pending_report_access_info_);
            pending_report_access_info_.clear();
        }
        
        // 如果有统计信息需要上报，则执行上报
        if (!info_to_report.empty()) {
            ReportStatistics(info_to_report);
        }
        
        // 等待下一个上报周期
        std::this_thread::sleep_for(std::chrono::milliseconds(report_interval));
    }
}

// 添加新的方法用于收集需要上报的统计信息
void EdgeServer::addStatsToReport(const uint32_t stream_unique_id, const uint32_t block_id, const double query_selectivity) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    pending_report_access_info_.push_back(cloud_edge_cache::BlockAccessInfo(stream_unique_id, block_id, query_selectivity));
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

    auto [start_block, end_block] = getBlockRange(table_name, start_timestamp, end_timestamp);
    auto stream_meta_exist = getStreamMeta(table_name); 
    
    // If schema doesn't exist, send the original query directly to center node
    if (!stream_meta_exist) {
        std::cout << "Stream metadata not found: " << table_name << ". Forwarding to center node." << std::endl;
        auto sub_response = executeSubQuery(center_addr_, sql_query, 0, 0);
        
        if (sub_response.status() == cloud_edge_cache::SubQueryResponse::OK) {
            response->CopyFrom(sub_response.result());
        }
        return grpc::Status::OK;
    }

    uint32_t stream_uniqueId = stream_meta_exist.value().unique_id_;
    
    // Query the cache index to find nodes with relevant data
    auto node_blocks = cache_index_->queryMainIndex(table_name, start_block, end_block, stream_uniqueId);
    // Create a map to group blocks by node
    std::map<std::string, std::vector<uint32_t>> node_to_blocks;
    std::vector<uint32_t> missing_blocks;

    for (uint32_t block_id = start_block; block_id <= end_block; block_id++) {
        if (node_blocks.find(block_id) == node_blocks.end()) {
            missing_blocks.push_back(block_id);
        } else {
            node_to_blocks[node_blocks[block_id]].push_back(block_id);
        }
    }

    // 创建异步查询任务
    std::vector<std::pair<QueryTask, std::future<cloud_edge_cache::SubQueryResponse>>> query_tasks;
    
    // 对每个节点的每个块发起异步查询
    for (const auto& [node_id, blocks] : node_to_blocks) {
        for (const auto& block : blocks) {
            std::string block_sql = addBlockConditions(sql_query, table_name, block);
            query_tasks.emplace_back(
                QueryTask{node_id, block_sql, block, stream_uniqueId},
                std::async(std::launch::async, 
                    [this, task = QueryTask{node_id, block_sql, block, stream_uniqueId}]() {
                        return executeSubQuery(task.node_id, task.sql_query, 
                                            task.block_id, task.stream_uniqueId);
                    }
                )
            );
        }
    }

    // 处理缺失的块，从中心节点查询
    for (const auto& block_id : missing_blocks) {
        std::string block_sql = addBlockConditions(sql_query, table_name, block_id);
        query_tasks.emplace_back(
            QueryTask{center_addr_, block_sql, block_id, stream_uniqueId},
            std::async(std::launch::async,
                [this, task = QueryTask{center_addr_, block_sql, block_id, stream_uniqueId}]() {
                    return executeSubQuery(task.node_id, task.sql_query, 
                                        task.block_id, task.stream_uniqueId);
                }
            )
        );
    }

    // 合并所有结果
    std::vector<cloud_edge_cache::QueryResponse> all_results;
    for (auto& [task, future] : query_tasks) {
        auto sub_response = future.get();
        
        switch (sub_response.status()) {
            case cloud_edge_cache::SubQueryResponse::OK:
                if (sub_response.has_result()) {
                    all_results.push_back(sub_response.result());
                }
                break;
                
            case cloud_edge_cache::SubQueryResponse::FALSE_POSITIVE:
                // 检测到假阳性，直接使用原始任务的SQL从中心节点重新获取数据
                std::cout << "False positive detected for block " << task.block_id 
                         << " on node " << task.node_id << ", retrying from center node" << std::endl;
                
                // 直接使用task中已有的SQL查询从中心节点重新查询
                auto retry_response = executeSubQuery(center_addr_, 
                                                    task.sql_query,  // 直接使用已处理好的SQL
                                                    task.block_id,
                                                    task.stream_uniqueId);
                
                if (retry_response.status() == cloud_edge_cache::SubQueryResponse::OK 
                    && retry_response.has_result()) {
                    all_results.push_back(retry_response.result());
                } else {
                    std::cerr << "Failed to retry query from center node for block " 
                             << task.block_id << std::endl;
                }
                break;
                
            case cloud_edge_cache::SubQueryResponse::ERROR:
                std::cerr << "SubQuery failed: " << sub_response.error_message() << std::endl;
                break;
        }
    }

    // 合并结果到最终响应
    if (!all_results.empty()) {
        // 设置列信息（使用第一个结果的列信息）
        const auto& first_result = all_results[0];
        for (const auto& col : first_result.columns()) {
            response->add_columns()->CopyFrom(col);
        }

        // 添加所有结果的行数据
        for (const auto& result : all_results) {
            for (const auto& row : result.rows()) {
                response->add_rows()->CopyFrom(row);
            }
        }
    }

    return grpc::Status::OK;
}

// Add new helper function to modify SQL query with block conditions
std::string EdgeServer::addBlockConditions(const std::string& original_sql, 
                                           const std::string& datastream_id,
                                           const uint32_t block_id) {
    // Parse original SQL
    hsql::SQLParserResult result;
    hsql::SQLParser::parse(original_sql, &result);
    
    if (!result.isValid() || result.size() != 1) {
        return original_sql;
    }

    const hsql::SQLStatement* stmt = result.getStatement(0);
    if (stmt->type() != hsql::kStmtSelect) {
        return original_sql;
    }

    // 获取数据源元数据
    auto stream_meta_exist = getStreamMeta(datastream_id);
    
    if (!stream_meta_exist) {
        std::cerr << "Stream metadata not found: " << datastream_id << std::endl;
        return original_sql;
    }

    Common::StreamMeta stream_meta = stream_meta_exist.value();

    // Add block-specific timestamp conditions
    std::stringstream modified_sql;
    modified_sql << original_sql;
    
    // If there's no WHERE clause, add one
    if (((const hsql::SelectStatement*)stmt)->whereClause == nullptr) {
        modified_sql << " WHERE ";
    } else {
        modified_sql << " AND ";
    }

    int64_t start_timestamp = stream_meta.start_time_ + block_id * stream_meta.time_range_;
    int64_t end_timestamp = stream_meta.start_time_ + (block_id + 1) * stream_meta.time_range_;

    modified_sql << "timestamp >= " << start_timestamp 
                << " AND timestamp < " << end_timestamp;
    
    return modified_sql.str();
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
    std::string src_node_addr = request->src_node_addr();
    std::cout << "Received metadata update from " << src_node_addr << std::endl;

    // Update local cache index with received metadata
    for (const auto& meta : request->stream_metadata()) {
        std::cout << "Updating metadata for stream: " << meta.datastream_id() << std::endl;
        cache_index_->updateIndex(src_node_addr, meta);
    }

    return grpc::Status::OK;
}

// -------------------------------------------------------------------------------------

grpc::Status EdgeServer::ReplaceCache(grpc::ServerContext* context,
                                     const cloud_edge_cache::CacheReplacement* request,
                                     cloud_edge_cache::Empty* response) {
    // 首先更新 schema
    for (const auto& meta : request->stream_metadata()) {
        Common::StreamMeta stream_meta;
        stream_meta.unique_id_ = meta.unique_id();
        stream_meta.start_time_ = meta.start_time();
        stream_meta.time_range_ = meta.time_range();
        
        typename tbb::concurrent_hash_map<std::string, Common::StreamMeta>::accessor accessor;
        schema_.insert(accessor, meta.datastream_id());
        accessor->second = stream_meta;
    }
    
    // 处理缓存操作
    for (const auto& op : request->block_operations()) {
        std::string block_key = std::to_string(op.datastream_unique_id()) + ":" + 
                               std::to_string(op.block_id());

        if (op.operation() == cloud_edge_cache::BlockOperation::ADD) {
            // 实现缓存添加逻辑
            std::cout << "Adding block to cache: " << block_key << std::endl;
            // TODO: 实现实际的缓存添加逻辑
        } else if (op.operation() == cloud_edge_cache::BlockOperation::REMOVE) {
            // 实现缓存删除逻辑
            std::cout << "Removing block from cache: " << block_key << std::endl;
            // TODO: 实现实际的缓存删除逻辑
        }
    }
    
    return grpc::Status::OK;
}

// -------------------------------------------------------------------------------------

void EdgeServer::Start(const std::string& server_address) {
    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(this); // Register both services

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    server_address_ = server_address;
    std::cout << "Edge server is running on " << server_address << std::endl;
    server->Wait();
}

// -------------------------------------------------------------------------------------

void EdgeServer::PushMetadataUpdate(const std::vector<std::string>& keys, const std::string& target_server_address) {
    // Create stub for the target edge server
    auto channel = grpc::CreateChannel(target_server_address, grpc::InsecureChannelCredentials());
    auto stub = cloud_edge_cache::EdgeToEdge::NewStub(channel);

    // Prepare request
    cloud_edge_cache::UpdateCacheMeta request;
    request.set_src_node_addr(ConfigManager::getInstance().getServerAddress());  // Set source address
    
    // Add metadata for each key
    for (const auto& key : keys) {
        auto metadata = cache_index_->getMetadata(key);
        if (metadata) {
            auto* stream_meta = request.add_stream_metadata();
            stream_meta->CopyFrom(*metadata);
        }
    }

    // Send request
    cloud_edge_cache::Empty response;
    grpc::ClientContext context;
    
    auto status = stub->UpdateMetadata(&context, request, &response);
    
    if (status.ok()) {
        std::cout << "Successfully pushed metadata update to " << target_server_address << std::endl;
    } else {
        std::cerr << "Failed to push metadata update to " << target_server_address 
                  << ": " << status.error_message() << std::endl;
    }
}

// -------------------------------------------------------------------------------------

void EdgeServer::ReportStatistics(const std::vector<cloud_edge_cache::BlockAccessInfo>& infos) {
    // 创建到中心服务器的存根  
    auto channel = grpc::CreateChannel(center_addr_, grpc::InsecureChannelCredentials());
    auto stub = cloud_edge_cache::EdgeToCenter::NewStub(channel);

    // 准备统计报告
    cloud_edge_cache::StatisticsReport request;
    request.set_server_address(server_address_);
    for (const auto& info : infos) {
        request.add_block_stats()->CopyFrom(info);
    }

    // 发送统计报告
    cloud_edge_cache::Empty response;
    grpc::ClientContext context;
    auto status = stub->ReportStatistics(&context, request, &response);
    if (status.ok()) {
        std::cout << "Successfully reported statistics to center server" << std::endl;
    } else {
        std::cerr << "Failed to report statistics: " << status.error_message() << std::endl;
    }
}

// -------------------------------------------------------------------------------------

grpc::Status EdgeServer::SubQuery(grpc::ServerContext* context,
                                 const cloud_edge_cache::QueryRequest* request,
                                 cloud_edge_cache::SubQueryResponse* response) {
    try {
        auto& config = ConfigManager::getInstance();
        std::string conn_str = config.getNodeDatabaseConfig(server_address_).getConnectionString();
        pqxx::connection conn(conn_str);

        // 首先构造并执行 EXISTS 查询
        std::string count_sql = "SELECT EXISTS (" + request->sql_query() + " LIMIT 1)";
        pqxx::work check_txn(conn);
        bool has_data = check_txn.query_value<bool>(count_sql);
        check_txn.commit();

        // 如果没有数据，返回假阳性响应
        if (!has_data) {
            response->set_status(cloud_edge_cache::SubQueryResponse::FALSE_POSITIVE);
            return grpc::Status::OK;
        }

        // 如果有数据，执行完整查询
        pqxx::work txn(conn);
        pqxx::result db_result = txn.exec(request->sql_query());
        txn.commit();

        // 设置响应状态
        response->set_status(cloud_edge_cache::SubQueryResponse::OK);
        
        // 填充查询结果
        auto* query_response = response->mutable_result();
        
        // 设置列信息
        if (!db_result.empty()) {
            for (const auto& column : db_result.columns()) {
                auto* col = query_response->add_columns();
                col->set_name(column.name());
                col->set_type(column.type());
            }

            // 添加行数据
            for (const auto& db_row : db_result) {
                auto* row = query_response->add_rows();
                for (size_t i = 0; i < db_row.size(); ++i) {
                    row->add_values(db_row[i].is_null() ? "" : db_row[i].c_str());
                }
            }
        }

        return grpc::Status::OK;
    } catch (const std::exception& e) {
        response->set_status(cloud_edge_cache::SubQueryResponse::ERROR);
        response->set_error_message(e.what());
        return grpc::Status::OK;
    }
}

// 修改原来的查询逻辑，使用 RPC 调用
cloud_edge_cache::SubQueryResponse EdgeServer::executeSubQuery(const std::string& node_id, 
                                                             const std::string& sql_query, 
                                                             const uint32_t block_id,
                                                             const uint32_t stream_unique_id) {
    auto channel = grpc::CreateChannel(node_id, grpc::InsecureChannelCredentials());
    auto stub = cloud_edge_cache::EdgeToEdge::NewStub(channel);

    cloud_edge_cache::QueryRequest request;
    request.set_sql_query(sql_query);

    cloud_edge_cache::SubQueryResponse response;
    grpc::ClientContext context;

    auto status = stub->SubQuery(&context, request, &response);

    if (!status.ok()) {
        throw std::runtime_error("RPC failed: " + status.error_message());
    }

    switch (response.status()) {
        case cloud_edge_cache::SubQueryResponse::FALSE_POSITIVE:
            // 处理假阳性情况，可以考虑从中心节点获取数据
            std::cout << "False positive detected for block " << block_id << " on node " << node_id << std::endl;
            break;
            
        case cloud_edge_cache::SubQueryResponse::OK:
            // 处理成功情况
            if (response.has_result()) {
                // 更新统计信息
                double selectivity = response.result().rows_size() / 
                                   stream_meta_exist.value().block_size_;
                addStatsToReport(stream_unique_id, block_id, selectivity);
            }
            break;
            
        case cloud_edge_cache::SubQueryResponse::ERROR:
            throw std::runtime_error("SubQuery error: " + response.error_message());
    }

    return response;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <ip_address> <port>" << std::endl;
        return 1;
    }

    const std::string ip_address = argv[1];
    const std::string port = argv[2];
    const std::string server_address = ip_address + ":" + port;
    
    EdgeServer edge_server;
    edge_server.Start(server_address);

    return 0;
}