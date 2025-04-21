#include "edge_server.h"

EdgeServer::EdgeServer() {
    auto& config = ConfigManager::getInstance();
    
    // 创建控制台和文件 sink
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(config.getLogFilePath(), true);

    // 创建多重 sink logger
    auto logger = std::make_shared<spdlog::logger>("edge_server_logger", spdlog::sinks_init_list{console_sink, file_sink});
    spdlog::set_default_logger(logger);

    // 设置日志级别
    std::string log_level = config.getEdgeLogLevel();
    if (log_level == "debug") {
        spdlog::set_level(spdlog::level::debug);
    } else {
        spdlog::set_level(spdlog::level::info);
    }

    // 其他初始化代码
    block_size_ = config.getBlockSizeMB() * 1024 * 1024;
    center_addr_ = config.getCenterAddress();
    center_latency_ = measureLatency(center_addr_);
    // // 测试专用
    // center_latency_ = 100;
    spdlog::info("Center node latency: {}ms", center_latency_);

    cache_index_ = std::make_unique<EdgeCacheIndex>();
    neighbor_addrs_.insert(center_addr_);
    cache_index_->setNodeLatency(server_address_, 0);
    cache_index_->setNodeLatency(center_addr_, center_latency_);
    cache_index_->setLatencyThreshold(config.getIndexLatencyThresholdMs());

    spdlog::debug("EdgeServer initialized with block size: {} bytes", block_size_);
}

EdgeServer::~EdgeServer() {
    // 停止统计信息上报线程
    should_stop_ = true;
    if (stats_report_thread_.joinable()) {
        stats_report_thread_.join();
    }
    spdlog::info("EdgeServer destroyed");
}

void EdgeServer::statsReportLoop() {
    auto& config = ConfigManager::getInstance();
    int64_t report_interval = config.getStatisticsReportInterval();
    
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
            spdlog::debug("Reported {} statistics", info_to_report.size());
        }
        
        // 等待下一个上报周期
        std::this_thread::sleep_for(std::chrono::seconds(report_interval));
    }
}

// 添加新的方法用于收集需要上报的统计信息
void EdgeServer::addStatsToReport(const uint32_t stream_unique_id, const uint32_t block_id, const double query_selectivity) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    // 创建新的 BlockAccessInfo 对象并设置字段值
    cloud_edge_cache::BlockAccessInfo info;
    info.set_datastream_unique_id(stream_unique_id);
    info.set_block_id(block_id);
    info.set_selectivity(query_selectivity);
    
    pending_report_access_info_.push_back(std::move(info));
}

// 新增：测量延迟的辅助函数
//这个实现使用 gRPC 通道建立连接来测量延迟，这比传统的 ICMP ping 
//更能反映实际的应用层延迟。每个节点都会进行3次测量并取平均值，以获得更稳定的结果。
int64_t EdgeServer::measureLatency(const std::string& address) {
    auto channel = grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
    auto stub = cloud_edge_cache::NetworkMetricsService::NewStub(channel);
    
    static constexpr int REPEAT_COUNT = 3;  // 测量次数
    int64_t total_latency = 0;
    int successful_measurements = 0;

    // 等待通道就绪
    if (!channel->WaitForConnected(gpr_time_add(
            gpr_now(GPR_CLOCK_REALTIME),
            gpr_time_from_seconds(5, GPR_TIMESPAN)))) {
        throw std::runtime_error("Failed to connect to node: " + address);
    }

    for (int i = 0; i < REPEAT_COUNT; ++i) {
        try {
            grpc::ClientContext context;
            cloud_edge_cache::ExecuteNetworkMetricsRequest request;
            cloud_edge_cache::ExecuteNetworkMetricsResponse response;
            
            auto start = std::chrono::high_resolution_clock::now();
            auto status = stub->ExecuteNetworkMeasurement(&context, request, &response);
            auto end = std::chrono::high_resolution_clock::now();
            
            if (status.ok()) {
                auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
                total_latency += latency;
                ++successful_measurements;
            }
            
            // 短暂休眠以避免过于频繁的请求
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
        } catch (const std::exception& e) {
            spdlog::error("Error measuring latency to {}: {}", address, e.what());
        }
    }

    if (successful_measurements == 0) {
        throw std::runtime_error("Failed to measure latency to node: " + address);
    }
    return total_latency / successful_measurements;
}

// -------------------------------------------------------------------------------------

grpc::Status EdgeServer::Query(grpc::ServerContext* context,
                               const cloud_edge_cache::QueryRequest* request,
                               cloud_edge_cache::QueryResponse* response) {
    // 解析SQL查询
    auto [table_name, start_timestamp, end_timestamp] = parseSQLQuery(request->sql_query());
    spdlog::info("Qirgin Query: {} Query for table: {}, timestamp range: [{}, {}]", request->sql_query(), table_name, start_timestamp, end_timestamp);

    auto stream_meta_exist = getStreamMeta(table_name);

    // 如果schema不存在，直接转发到中心节点
    if (!stream_meta_exist) {
        spdlog::info("Stream metadata not found: {}. Forwarding to center node.", table_name);
        auto sub_response = executeSubQuery(center_addr_, request->sql_query(), 0, 0);
        if (sub_response.status() == cloud_edge_cache::SubQueryResponse::OK) {
            response->CopyFrom(sub_response.result());
        }
        return grpc::Status::OK;
    }
    
    // 获取数据块范围
    auto [start_block, end_block] = getBlockRange(table_name, start_timestamp, end_timestamp);
    
    spdlog::debug(" start_block {} end_block {}", start_block, end_block);

    // 创建并执行分布式查询任务
    auto query_tasks = createQueryTasks(request->sql_query(), 
                                      table_name, 
                                      start_block, 
                                      end_block,
                                      stream_meta_exist.value().unique_id_);

    // 收集并处理查询结果
    auto all_results = processQueryResults(query_tasks);

    // 合并结果
    mergeQueryResults(response, all_results);

    return grpc::Status::OK;
}

// 新增：处理查询任务结果的函数（如果出现假阳性，则从中心节点重新查询）
std::vector<cloud_edge_cache::QueryResponse> EdgeServer::processQueryResults(
    std::vector<std::pair<EdgeServer::QueryTask, std::future<cloud_edge_cache::SubQueryResponse>>>& query_tasks) {
    
    std::vector<cloud_edge_cache::QueryResponse> all_results;
    for (auto& [task, future] : query_tasks) {
        auto sub_response = future.get();
        
        switch (sub_response.status()) {
            case cloud_edge_cache::SubQueryResponse_Status_OK: {
                if (sub_response.has_result()) {
                    all_results.push_back(sub_response.result());
                }
                break;
            }
                
            case cloud_edge_cache::SubQueryResponse_Status_FALSE_POSITIVE: {
                spdlog::info("False positive detected for block {} on node {}, retrying from center node", task.block_id, task.node_id);
                
                auto retry_response = executeSubQuery(center_addr_, 
                                                    task.sql_query,
                                                    task.block_id,
                                                    task.stream_uniqueId);

                if (retry_response.status() == cloud_edge_cache::SubQueryResponse_Status_OK 
                    && retry_response.has_result()) {
                    all_results.push_back(retry_response.result());
                } else {
                    spdlog::error("Failed to retry query from center node for block {}", task.block_id);
                }
                break;
            }
                
            case cloud_edge_cache::SubQueryResponse_Status_ERROR: {
                spdlog::error("SubQuery failed: {}", sub_response.error_message());
                break;
            }
        }
    }
    return all_results;
}

// 解析SQL查询并提取时间范围
std::tuple<std::string, int64_t, int64_t> EdgeServer::parseSQLQuery(const std::string& sql_query) {
    hsql::SQLParserResult result;
    hsql::SQLParser::parse(sql_query, &result);
    
    int64_t start_timestamp = std::numeric_limits<int64_t>::min();
    int64_t end_timestamp = std::numeric_limits<int64_t>::max();
    std::string table_name;

    if (result.isValid()) {
        const hsql::SQLStatement* stmt = result.getStatement(0);
        if (stmt->type() == hsql::kStmtSelect) {
            const hsql::SelectStatement* select = (const hsql::SelectStatement*) stmt;
            if (select->fromTable) {
                table_name = select->fromTable->getName();
                // std::cout << "EdgeServer::parseSQLQuery table_name: " << select->fromTable->getName() << std::endl;
            }
            if (select->whereClause != nullptr) {
                parseWhereClause(select->whereClause, start_timestamp, end_timestamp);
                // std::cout << "EdgeServer::parseSQLQuery start_time " << start_timestamp << " end_time " << end_timestamp << std::endl;
            }
        }
    }

    return {table_name, start_timestamp, end_timestamp};
}

// 创建并执行分布式查询任务
std::vector<std::pair<EdgeServer::QueryTask, std::future<cloud_edge_cache::SubQueryResponse>>> 
EdgeServer::createQueryTasks(const std::string& sql_query, 
                           const std::string& table_name,
                           uint32_t start_block, 
                           uint32_t end_block,
                           uint32_t stream_uniqueId) {
    // Query the cache index to find nodes with relevant data
    auto& node_blocks = cache_index_->queryMainIndex(table_name, start_block, end_block, stream_uniqueId);
    std::map<std::string, std::vector<uint32_t>> node_to_blocks;
    std::vector<uint32_t> missing_blocks;

    // Group blocks by node
    for (uint32_t block_id = start_block; block_id <= end_block; block_id++) {
        typename tbb::concurrent_hash_map<uint32_t, std::string>::const_accessor accessor;
        if (!node_blocks.find(accessor, block_id)) {
            missing_blocks.push_back(block_id);
            spdlog::info("missing blocks + {}", block_id);
        } else {
            node_to_blocks[accessor->second].push_back(block_id);
            spdlog::info("finding blocks + {} in {}", block_id, accessor->second);
        }
        // accessor 会在作用域结束时自动释放
    }

    std::vector<std::pair<EdgeServer::QueryTask, std::future<cloud_edge_cache::SubQueryResponse>>> query_tasks;
    
    // Create tasks for each node's blocks
    for (const auto& [node_id, blocks] : node_to_blocks) {
        for (const auto& block : blocks) {
            std::string block_sql = addBlockConditions(sql_query, table_name, block);
            query_tasks.emplace_back(
                EdgeServer::QueryTask{node_id, block_sql, block, stream_uniqueId},
                std::async(std::launch::async, 
                    [this, task = EdgeServer::QueryTask{node_id, block_sql, block, stream_uniqueId}]() {
                        return executeSubQuery(task.node_id, task.sql_query, 
                                            task.block_id, task.stream_uniqueId);
                    }
                )
            );
        }
    }

    // Create tasks for missing blocks from center node
    for (const auto& block_id : missing_blocks) {
        std::string block_sql = addBlockConditions(sql_query, table_name, block_id);
        query_tasks.emplace_back(
            EdgeServer::QueryTask{center_addr_, block_sql, block_id, stream_uniqueId},
            std::async(std::launch::async,
                [this, task = EdgeServer::QueryTask{center_addr_, block_sql, block_id, stream_uniqueId}]() {
                    return executeSubQuery(task.node_id, task.sql_query, 
                                        task.block_id, task.stream_uniqueId);
                }
            )
        );
    }

    return query_tasks;
}

// 合并查询结果
void EdgeServer::mergeQueryResults(cloud_edge_cache::QueryResponse* response,
                                 const std::vector<cloud_edge_cache::QueryResponse>& all_results) {
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
}

// 新增：辅助函数，用于处理表达式
std::string EdgeServer::expressionToString(const hsql::Expr* expr) {
    if (!expr) return "";
    
    switch (expr->type) {
        case hsql::kExprStar:
            return "*";
            
        case hsql::kExprColumnRef:
            return expr->table ? 
                   std::string(expr->table) + "." + expr->name :
                   expr->name;
            
        case hsql::kExprLiteralFloat:
            return std::to_string(expr->fval);
            
        case hsql::kExprLiteralInt:
            return std::to_string(expr->ival);
            
        case hsql::kExprLiteralString:
            return "'" + std::string(expr->name) + "'";
            
        case hsql::kExprFunctionRef: {
            std::string func_str = expr->name;
            func_str += "(";
            if (expr->exprList != nullptr) {
                for (size_t i = 0; i < expr->exprList->size(); ++i) {
                    if (i > 0) func_str += ", ";
                    func_str += expressionToString(expr->exprList->at(i));
                }
            }
            func_str += ")";
            return func_str;
        }
            
        case hsql::kExprOperator: {
            std::string op_str;
            // 处理一元运算符
            if (expr->opType == hsql::kOpNot) {
                return "NOT " + expressionToString(expr->expr);
            }
            
            // 处理二元运算符
            std::string left = expressionToString(expr->expr);
            std::string right = expressionToString(expr->expr2);
            
            // 处理特殊情况：BETWEEN
            if (expr->opType == hsql::kOpBetween) {
                return left + " BETWEEN " + right + " AND " + 
                       expressionToString(expr->expr2);
            }
            
            // 处理 IS NULL 和 IS NOT NULL
            if (expr->opType == hsql::kOpIsNull) {
                return left + " IS NULL";
            }
            if (expr->opType == hsql::kOpIsNull) {
                return left + " IS NOT NULL";
            }
            
            // 处理常规二元运算符
            switch (expr->opType) {
                case hsql::kOpPlus:   return left + " + " + right;
                case hsql::kOpMinus:  return left + " - " + right;
                case hsql::kOpAsterisk: return left + " * " + right;
                case hsql::kOpSlash:  return left + " / " + right;
                case hsql::kOpEquals: return left + " = " + right;
                case hsql::kOpNotEquals: return left + " != " + right;
                case hsql::kOpLess:   return left + " < " + right;
                case hsql::kOpLessEq: return left + " <= " + right;
                case hsql::kOpGreater: return left + " > " + right;
                case hsql::kOpGreaterEq: return left + " >= " + right;
                case hsql::kOpAnd:    return "(" + left + " AND " + right + ")";
                case hsql::kOpOr:     return "(" + left + " OR " + right + ")";
                case hsql::kOpLike:   return left + " LIKE " + right;
                case hsql::kOpNotLike: return left + " NOT LIKE " + right;
                case hsql::kOpIn:     return left + " IN " + right;
                case hsql::kOpNot:    return left + " NOT IN " + right;
                default:              return left + " ?? " + right;
            }
        }
            
        case hsql::kExprSelect:
            // 处理子查询
            return "(" + subqueryToString(expr->select) + ")";
            
        default:
            return expr->name ? expr->name : "";
    }
}

// 新增：处理子查询的辅助函数
std::string EdgeServer::subqueryToString(const hsql::SelectStatement* select) {
    std::stringstream ss;
    
    // SELECT 子句
    ss << "SELECT ";
    if (select->selectList != nullptr) {
        for (size_t i = 0; i < select->selectList->size(); ++i) {
            if (i > 0) ss << ", ";
            ss << expressionToString(select->selectList->at(i));
        }
    }
    
    // FROM 子句
    if (select->fromTable != nullptr) {
        ss << " FROM " << select->fromTable->getName();
    }
    
    // WHERE 子句
    if (select->whereClause != nullptr) {
        ss << " WHERE " << expressionToString(select->whereClause);
    }
    
    // GROUP BY 子句
    if (select->groupBy != nullptr) {
        ss << " GROUP BY ";
        for (size_t i = 0; i < select->groupBy->columns->size(); ++i) {
            if (i > 0) ss << ", ";
            ss << expressionToString(select->groupBy->columns->at(i));
        }
    }
    
    // ORDER BY 子句
    if (select->order != nullptr) {
        ss << " ORDER BY ";
        for (const auto* order_desc : *select->order) {
            ss << expressionToString(order_desc->expr);
            if (order_desc->type == hsql::kOrderDesc) {
                ss << " DESC";
            }
        }
    }
    
    // LIMIT 子句
    if (select->limit != nullptr) {
        ss << " LIMIT " << select->limit->limit;
        if (select->limit->offset != nullptr) {
            ss << " OFFSET " << select->limit->offset->ival;
        }
    }
    
    return ss.str();
}

// 修改后的 addBlockConditions 方法
std::string EdgeServer::addBlockConditions(const std::string& original_sql, 
                                         const std::string& datastream_id,
                                         const uint32_t block_id) {
    hsql::SQLParserResult result;
    hsql::SQLParser::parse(original_sql, &result);
    
    if (!result.isValid() || result.size() != 1) {
        return original_sql;
    }

    const hsql::SQLStatement* stmt = result.getStatement(0);
    if (stmt->type() != hsql::kStmtSelect) {
        return original_sql;
    }

    const hsql::SelectStatement* select = static_cast<const hsql::SelectStatement*>(stmt);

    // 获取数据源元数据
    auto stream_meta_exist = getStreamMeta(datastream_id);
    if (!stream_meta_exist) {
        spdlog::error("Stream metadata not found: {}", datastream_id);
        return original_sql;
    }

    Common::StreamMeta stream_meta = stream_meta_exist.value();

    // 计算块的时间范围
    int64_t start_timestamp = stream_meta.start_time_ + block_id * stream_meta.time_range_;
    int64_t end_timestamp = start_timestamp + stream_meta.time_range_;
    
    // std::cout << "Block " << block_id << " time range calculation:" << std::endl
    //           << "  start_time_: " << stream_meta.start_time_ << std::endl
    //           << "  time_range_: " << stream_meta.time_range_ << std::endl
    //           << "  start_timestamp: " << start_timestamp << std::endl
    //           << "  end_timestamp: " << end_timestamp << std::endl;

    // 检查时间范围的有效性
    if (end_timestamp <= start_timestamp) {
        spdlog::error("Error: Invalid time range calculation for block {}: end_timestamp <= start_timestamp", block_id);
        return original_sql;
    }

    std::stringstream modified_sql;
    
    // SELECT 子句
    modified_sql << "SELECT ";
    if (select->selectList != nullptr) {
        for (size_t i = 0; i < select->selectList->size(); ++i) {
            if (i > 0) modified_sql << ", ";
            modified_sql << expressionToString(select->selectList->at(i));
        }
    } else {
        modified_sql << "*";
    }

    // FROM 子句
    modified_sql << " FROM ";
    if (select->fromTable != nullptr) {
        modified_sql << select->fromTable->getName();
    }

    // WHERE 子句
    modified_sql << " WHERE ";
    if (select->whereClause != nullptr) {
        modified_sql << "(" << expressionToString(select->whereClause) << ") AND ";
    }
    modified_sql << "date_time >= " << start_timestamp 
                << " AND date_time < " << end_timestamp;

    // GROUP BY 子句
    if (select->groupBy != nullptr) {
        modified_sql << " GROUP BY ";
        for (size_t i = 0; i < select->groupBy->columns->size(); ++i) {
            if (i > 0) modified_sql << ", ";
            modified_sql << expressionToString(select->groupBy->columns->at(i));
        }
    }

    // ORDER BY 子句
    if (select->order != nullptr) {
        modified_sql << " ORDER BY ";
        for (const auto* order_desc : *select->order) {
            modified_sql << expressionToString(order_desc->expr);
            if (order_desc->type == hsql::kOrderDesc) {
                modified_sql << " DESC";
            }
        }
    }

    // LIMIT 子句
    if (select->limit != nullptr && select->limit->limit != nullptr) {  // 检查 limit->limit 是否为空
        if (select->limit->limit->type == hsql::kExprLiteralInt) {
            modified_sql << " LIMIT " << select->limit->limit->ival;
            
            // 处理 OFFSET
            if (select->limit->offset != nullptr && 
                select->limit->offset->type == hsql::kExprLiteralInt) {
                modified_sql << " OFFSET " << select->limit->offset->ival;
            }
        }
    }

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
            spdlog::warn("Warning: OR condition detected, using widest possible date_time range");
            start_timestamp = std::numeric_limits<int64_t>::min();
            end_timestamp = std::numeric_limits<int64_t>::max();
        } else {
            // 处理比较操作符
            if (expr->expr && expr->expr->type == hsql::kExprColumnRef &&
                std::string(expr->expr->name) == "date_time") {
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
    spdlog::debug("Received metadata update from {}", src_node_addr);
    // 处理块操作
    updateBlockOperations(request->block_operations(), src_node_addr);

    return grpc::Status::OK;
}

// 修改后的 updateSchemaInfo 函数
void EdgeServer::updateSchemaInfo(const google::protobuf::RepeatedPtrField<cloud_edge_cache::StreamMetadata>& metadata) {
    for (const auto& meta : metadata) {
        Common::StreamMeta stream_meta;
        stream_meta.datastream_id_ = meta.datastream_id();
        stream_meta.unique_id_ = meta.unique_id();
        stream_meta.start_time_ = meta.start_time();
        stream_meta.time_range_ = meta.time_range();
        spdlog::info("update schema for table {} unique_id_ {} start_time_{}", stream_meta.datastream_id_, stream_meta.unique_id_, stream_meta.start_time_);
        
        // Check if the schema already exists
        typename tbb::concurrent_hash_map<std::string, Common::StreamMeta>::accessor accessor;
        if (!schema_.find(accessor, meta.datastream_id())) {
            // Update schema if it does not exist
            schema_.insert(accessor, meta.datastream_id());
            accessor->second = stream_meta;
        }

        // Check if the reverse mapping already exists
        typename tbb::concurrent_hash_map<uint32_t, std::string>::accessor id_accessor;
        if (!unique_id_to_datastream_.find(id_accessor, meta.unique_id())) {
            // Update reverse mapping if it does not exist
            unique_id_to_datastream_.insert(id_accessor, meta.unique_id());
            id_accessor->second = meta.datastream_id();
        }
    }
}

// 新增：处理块操作的函数
void EdgeServer::updateBlockOperations(
    const google::protobuf::RepeatedPtrField<cloud_edge_cache::BlockOperationInfo>& operations,
    const std::string& src_node_addr) {
    
    for (const auto& op : operations) {
        if (op.operation() == cloud_edge_cache::BlockOperation::ADD) {
            cache_index_->addBlock(op.block_id(), 
                                 src_node_addr,
                                 op.datastream_unique_id());
            spdlog::debug("Added block meta {} for stream {} from node {}", op.block_id(), op.datastream_unique_id(), src_node_addr);
        } else if (op.operation() == cloud_edge_cache::BlockOperation::REMOVE) {
            cache_index_->removeBlock(op.block_id(),
                                    src_node_addr,
                                    op.datastream_unique_id());
            spdlog::debug("Removed block meta{} for stream {} from node {}", op.block_id(), op.datastream_unique_id(), src_node_addr);
        }
    }
}

// -------------------------------------------------------------------------------------

// 确保表存在，如果不存在则创建
void EdgeServer::ensureTableExists(const std::string& table_name, 
                                 pqxx::connection& local_conn,
                                 pqxx::connection& center_conn) {
    pqxx::work check_txn(local_conn);
    std::string check_table_query = 
        "SELECT EXISTS ("
        "SELECT FROM information_schema.tables "
        "WHERE table_name = " + check_txn.quote(table_name) + ")";
    
    bool table_exists = check_txn.query_value<bool>(check_table_query);
    check_txn.commit();
    
    if (!table_exists) {
        spdlog::info("Table {} does not exist, creating...", table_name);
        
        // 从中心节点获取表结构
        pqxx::work center_txn(center_conn);
        std::string get_schema_query = 
            "SELECT column_name, data_type, character_maximum_length "
            "FROM information_schema.columns "
            "WHERE table_name = " + center_txn.quote(table_name) + 
            " ORDER BY ordinal_position";
        
        pqxx::result schema_info = center_txn.exec(get_schema_query);
        center_txn.commit();
        
        if (schema_info.empty()) {
            throw std::runtime_error("Failed to get table schema from center node");
        }
        
        // 构造并执行创建表的SQL
        std::string create_table_query = buildCreateTableQuery(table_name, schema_info);
        spdlog::info("create_table_query = {}", create_table_query);
        pqxx::work create_txn(local_conn);
        create_txn.exec(create_table_query);
        create_txn.commit();
    }
}

// 构建创建表的SQL语句
std::string EdgeServer::buildCreateTableQuery(const std::string& table_name, 
                                            const pqxx::result& schema_info) {
    std::string query = "CREATE TABLE IF NOT EXISTS " + table_name + " (";
    for (size_t i = 0; i < schema_info.size(); ++i) {
        if (i > 0) query += ", ";
        
        std::string column_name = schema_info[i][0].as<std::string>();
        std::string data_type = schema_info[i][1].as<std::string>();
        
        query += column_name + " " + data_type;
        
        if (!schema_info[i][2].is_null()) {
            query += "(" + std::to_string(schema_info[i][2].as<int>()) + ")";
        }
    }
    query += ")";
    return query;
}

// 添加数据块
void EdgeServer::addDataBlock(const std::string& table_name,
                            uint64_t block_start_time,
                            uint64_t block_end_time,
                            pqxx::connection& local_conn,
                            pqxx::connection& center_conn) {
    pqxx::work center_txn(center_conn);
    pqxx::work local_txn(local_conn);
    
    std::string select_query = 
        "SELECT * FROM " + table_name + 
        " WHERE date_time >= " + std::to_string(block_start_time) + 
        " AND date_time < " + std::to_string(block_end_time);
    
    pqxx::result rows = center_txn.exec(select_query);
    spdlog::debug("addDataBlock select_query = {}", select_query);
    if (!rows.empty()) {
        std::string insert_query = buildInsertQuery(table_name, rows);
        // spdlog::debug("addDataBlock insert_query = {}", insert_query);
        local_txn.exec(insert_query);
        local_txn.commit();
    }
    
    center_txn.commit();
}

// 构建插入数据的SQL语句
std::string EdgeServer::buildInsertQuery(const std::string& table_name, 
                                       const pqxx::result& rows) {
    std::string query = "INSERT INTO " + table_name + " ("; 
    for (size_t i = 0; i < rows.columns(); ++i) {
        if (i > 0) query += ", ";
        query += rows.column_name(i);
    }
    
    query += ") VALUES ";
    
    for (size_t i = 0; i < rows.size(); ++i) {
        if (i > 0) query += ", ";
        query += "(";
        
        for (size_t j = 0; j < rows.columns(); ++j) {
            if (j > 0) query += ", ";
            if (rows[i][j].is_null())
                query += "NULL";
            else
                query += "\'" + pqxx::to_string(rows[i][j]) + "\'";
        }
        
        query += ")";
    }
    
    return query;
}

// 删除数据块
void EdgeServer::removeDataBlock(const std::string& table_name,
                               uint64_t block_start_time,
                               uint64_t block_end_time,
                               pqxx::connection& local_conn) {
    pqxx::work local_txn(local_conn);
    std::string delete_query = 
        "DELETE FROM " + table_name + 
        " WHERE date_time >= " + std::to_string(block_start_time) + 
        " AND date_time < " + std::to_string(block_end_time);
    spdlog::debug("removeDataBlock delete_query = {}", delete_query);
    local_txn.exec(delete_query);
    local_txn.commit();
}

// 主函数
grpc::Status EdgeServer::ReplaceCache(grpc::ServerContext* context,
                                     const cloud_edge_cache::CacheReplacement* request,
                                     cloud_edge_cache::Empty* response) {
    // 向邻居节点推送元数据更新，块操作信息
    cloud_edge_cache::UpdateCacheMeta update_meta;
    update_meta.set_src_node_addr(server_address_);
    try {

        auto& config = ConfigManager::getInstance();
        std::string local_conn_str = config.getDatabaseConfig().getConnectionString();
        std::string center_conn_str = config.getNodeDatabaseConfig(center_addr_).getConnectionString();
        
        pqxx::connection local_conn(local_conn_str);
        pqxx::connection center_conn(center_conn_str);

        // 处理每个块操作
        for (const auto& op : request->block_operations()) {
            auto datastream_id = getDatastreamId(op.datastream_unique_id());
            if (!datastream_id.has_value()) {
                throw std::runtime_error("Datastream ID not found");
            }
            auto stream_meta = getStreamMeta(datastream_id.value());
            std::string table_name = datastream_id.value();
            
            if (stream_meta) { // 检查是否有值
                uint64_t block_start_time = stream_meta->start_time_ + op.block_id() * stream_meta->time_range_;
                uint64_t block_end_time = block_start_time + stream_meta->time_range_;
                
                try {
                    if (op.operation() == cloud_edge_cache::BlockOperation::ADD) {
                        ensureTableExists(table_name, local_conn, center_conn);
                        spdlog::info("addDataBlock table_name = {}, datastream_unique_id = {}, block_id = {}", table_name, op.datastream_unique_id(), op.block_id());
                        addDataBlock(table_name, block_start_time, block_end_time, 
                                   local_conn, center_conn);
                    } else if (op.operation() == cloud_edge_cache::BlockOperation::REMOVE) {
                        spdlog::info("removeDataBlock table_name = {}, datastream_unique_id = {}, block_id = {}", table_name, op.datastream_unique_id(), op.block_id());
                        removeDataBlock(table_name, block_start_time, block_end_time, 
                                      local_conn);
                    }
                } catch (const std::exception& e) {
                    spdlog::error("Error processing block operation: {}", e.what());
                    return grpc::Status(grpc::StatusCode::INTERNAL, 
                                      "Failed to process block operation: " + std::string(e.what()));
                }
                auto* block_op = update_meta.add_block_operations();
                block_op->set_datastream_unique_id(op.datastream_unique_id());
                block_op->set_block_id(op.block_id());
                block_op->set_operation(op.operation());
            } else {
                throw std::runtime_error("Stream metadata not found");
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Database connection error: {}", e.what());
        return grpc::Status(grpc::StatusCode::INTERNAL, 
                          "Database connection error: " + std::string(e.what()));
    }
    
    // 向所有邻居节点推送更新
    for (const auto& neighbor : neighbor_addrs_) {
        spdlog::debug("PushMetadataUpdate to neighbor {}", neighbor);
        PushMetadataUpdate(update_meta, neighbor);
    }

    return grpc::Status::OK;
}

// -------------------------------------------------------------------------------------

void EdgeServer::Start(const std::string& server_address) {
    server_address_ = server_address;

    // 启动统计信息上报线程
    should_stop_ = false;
    stats_report_thread_ = std::thread(&EdgeServer::statsReportLoop, this);

    // 启动服务器
    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    // 分别注册每个服务
    builder.RegisterService(static_cast<cloud_edge_cache::ClientToEdge::Service*>(this));
    builder.RegisterService(static_cast<cloud_edge_cache::EdgeToEdge::Service*>(this));
    builder.RegisterService(static_cast<cloud_edge_cache::CenterToEdge::Service*>(this));
    builder.RegisterService(static_cast<cloud_edge_cache::NetworkMetricsService::Service*>(this));
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    spdlog::info("Edge server is running on {}", server_address);
    // 向中心节点注册
    registerWithCenter();
    server->Wait();
}

// -------------------------------------------------------------------------------------

void EdgeServer::PushMetadataUpdate(const cloud_edge_cache::UpdateCacheMeta& update_meta,
                                  const std::string& target_server_address) {
    auto channel = grpc::CreateChannel(target_server_address, grpc::InsecureChannelCredentials());
    auto stub = cloud_edge_cache::EdgeToEdge::NewStub(channel);

    cloud_edge_cache::Empty response;
    grpc::ClientContext context;
    
    auto status = stub->UpdateMetadata(&context, update_meta, &response);
    
    if (status.ok()) {
        spdlog::debug("Successfully pushed metadata update to {}", target_server_address);
    } else {
        spdlog::error("Failed to push metadata update to {}: {}", target_server_address, status.error_message());
    }
}

// -------------------------------------------------------------------------------------

void EdgeServer::ReportStatistics(std::vector<cloud_edge_cache::BlockAccessInfo>& infos) {
    // 创建到中心服务器的存根  
    auto channel = grpc::CreateChannel(center_addr_, grpc::InsecureChannelCredentials());
    auto stub = cloud_edge_cache::EdgeToCenter::NewStub(channel);

    // 准备统计报告
    cloud_edge_cache::StatisticsReport request;
    request.set_server_address(server_address_);
    for (const auto& info : infos) {
        request.add_block_stats()->CopyFrom(info);
    }
    // std::cout << "ReportStatistics request" << std::endl;
    // 发送统计报告
    cloud_edge_cache::Empty response;
    grpc::ClientContext context;
    auto status = stub->ReportStatistics(&context, request, &response);
    if (status.ok()) {
        spdlog::debug("Successfully reported statistics to center server");
    } else {
        spdlog::error("Failed to report statistics: {}", status.error_message());
    }
}

// -------------------------------------------------------------------------------------

grpc::Status EdgeServer::SubQuery(grpc::ServerContext* context,
                                const cloud_edge_cache::QueryRequest* request,
                                cloud_edge_cache::SubQueryResponse* response) {
    try {
        auto& config = ConfigManager::getInstance();
        std::string conn_str = config.getDatabaseConfig().getConnectionString();
        pqxx::connection conn(conn_str);
        
        // 获取数据流元数据
        auto datastream_id = getDatastreamId(request->stream_unique_id());
        if (!datastream_id) {
            throw std::runtime_error("Datastream ID not found");
        }

        auto stream_meta = getStreamMeta(datastream_id.value());
        if (!stream_meta) {
            throw std::runtime_error("Stream metadata not found");
        }

        // 计算块的时间范围
        int64_t start_timestamp = stream_meta->start_time_ + 
                                request->block_id() * stream_meta->time_range_;
        int64_t end_timestamp = start_timestamp + stream_meta->time_range_;

        // 构造简单的 EXISTS 查询
        std::string exists_sql = 
            "SELECT EXISTS (SELECT 1 FROM " + datastream_id.value() + 
            " WHERE date_time >= " + std::to_string(start_timestamp) + 
            " AND date_time < " + std::to_string(end_timestamp) + ")";

        // 执行 EXISTS 查询
        pqxx::work check_txn(conn);
        bool has_data = check_txn.query_value<bool>(exists_sql);
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
            for (int i = 0; i < db_result.columns(); ++i) {
                auto* col = query_response->add_columns();
                col->set_name(db_result.column_name(i));
                col->set_type(std::to_string(db_result.column_type(i)));
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
        spdlog::error("Error in SubQuery: {}", e.what());
        response->set_status(cloud_edge_cache::SubQueryResponse::ERROR);
        response->set_error_message(e.what());
        return grpc::Status(grpc::StatusCode::INTERNAL, e.what());
    }
}

// 修改原来的查询逻辑，使用 RPC 调用
cloud_edge_cache::SubQueryResponse EdgeServer::executeSubQuery(const std::string& node_id, 
                                                             const std::string& sql_query, 
                                                             const uint32_t block_id,
                                                             const uint32_t stream_unique_id) {
    auto channel = grpc::CreateChannel(node_id, grpc::InsecureChannelCredentials());
    auto stub = cloud_edge_cache::EdgeToEdge::NewStub(channel);

    // 添加通道状态检查
    if (!channel->WaitForConnected(gpr_time_add(
            gpr_now(GPR_CLOCK_REALTIME),
            gpr_time_from_seconds(5, GPR_TIMESPAN)))) {
        spdlog::error("Failed to connect to node: {}", node_id);
        throw std::runtime_error("Connection failed");
    }
    
    cloud_edge_cache::QueryRequest request;
    request.set_sql_query(sql_query);
    request.set_block_id(block_id);                    // 设置 block_id
    request.set_stream_unique_id(stream_unique_id);    // 设置 stream_unique_id

    cloud_edge_cache::SubQueryResponse response;
    grpc::ClientContext context;

    // std::cout << "Executing SubQuery RPC to " << node_id << std::endl;
    auto status = stub->SubQuery(&context, request, &response);

    if (!status.ok()) {
        spdlog::error("RPC failed: {} (code={})", status.error_message(), status.error_code());
        throw std::runtime_error("RPC failed: " + status.error_message());
    }

    switch (response.status()) {
        case cloud_edge_cache::SubQueryResponse::FALSE_POSITIVE:
            // 处理假阳性情况，可以考虑从中心节点获取数据
            spdlog::info("False positive detected for block {} on node {}", block_id, node_id);
            break;
            
        case cloud_edge_cache::SubQueryResponse::OK:
            // 处理成功情况
            // TODO(zhengfuyu): 假设这种case是schema_不存在，向中心云访问schema_的情况呢？
            if (response.has_result()) {
                auto datastream_id = getDatastreamId(stream_unique_id);
                if (datastream_id) {
                    auto stream_meta = getStreamMeta(datastream_id.value());
                    if (stream_meta) {
                        // Calculate average row size using a sample of rows
                        size_t total_sample_size = 0;
                        size_t sample_count = std::min(10, response.result().rows_size()); // Sample up to 10 rows
                        
                        for (int i = 0; i < sample_count; i++) {
                            const auto& row = response.result().rows(i);
                            size_t row_size = 0;
                            for (const auto& field : row.values()) {
                                // Add base storage overhead plus actual field content
                                row_size += sizeof(std::string) + field.capacity();
                            }
                            total_sample_size += row_size;
                        }
                        
                        // Calculate average row size and estimate total
                        double avg_row_size = sample_count > 0 ? 
                            static_cast<double>(total_sample_size) / sample_count : 0;
                        size_t estimated_total_size = avg_row_size * response.result().rows_size();
                        
                        double selectivity = estimated_total_size / static_cast<double>(block_size_);
                        addStatsToReport(stream_unique_id, block_id, selectivity);
                    }
                }
            }
            break;
            
        case cloud_edge_cache::SubQueryResponse::ERROR:
            throw std::runtime_error("SubQuery error: " + response.error_message());
    }

    return response;
}

void EdgeServer::registerWithCenter() {
    auto channel = grpc::CreateChannel(center_addr_, grpc::InsecureChannelCredentials());
    auto stub = cloud_edge_cache::EdgeToCenter::NewStub(channel);
    
    cloud_edge_cache::RegisterRequest request;
    request.set_node_address(server_address_);
    
    cloud_edge_cache::Empty response;
    grpc::ClientContext context;
    
    auto status = stub->Register(&context, request, &response);
    if (!status.ok()) {
        throw std::runtime_error("Failed to register with center node: " + status.error_message());
    }
    spdlog::info("Successfully registered with center node");
}

// 实现更新集群节点信息的 gRPC 方法
grpc::Status EdgeServer::UpdateClusterNodes(grpc::ServerContext* context,
                                          const cloud_edge_cache::ClusterNodesUpdate* request,
                                          cloud_edge_cache::Empty* response) {
    std::set<std::string> new_neighbors;

    // 获取当前的邻居节点列表（用于检查）
    std::set<std::string> current_neighbors;
    {
        std::lock_guard<std::mutex> lock(nodes_mutex_);
        current_neighbors = neighbor_addrs_;
    }

    // 处理所有节点
    for (const auto& node : request->nodes()) {
        std::string node_addr = node.node_address();
        try {
            int64_t node_latency = 0;
            
            // 检查是否是已知的邻居节点
            if (current_neighbors.find(node_addr) != current_neighbors.end()) {
                // 已知邻居节点，直接使用缓存的延迟值
                node_latency = cache_index_->getNodeLatency(node_addr);
                spdlog::debug("Using cached latency for node {}: {}ms", node_addr, node_latency);
            } else {
                // 新节点，需要测量延迟
                node_latency = measureLatency(node_addr);
                spdlog::debug("Measured new node {} latency: {}ms", node_addr, node_latency);
            }

            // 如果节点延迟小于中心节点延迟，将其添加为邻居
            if (node_latency < center_latency_) {
                new_neighbors.insert(node_addr);
                cache_index_->setNodeLatency(node_addr, node_latency);
            }
        } catch (const std::exception& e) {
            spdlog::error("Failed to measure latency to {}: {}", node_addr, e.what());
        }
    }

    // 更新 schema 信息
    updateSchemaInfo(request->stream_metadata());

    {
        std::lock_guard<std::mutex> lock(nodes_mutex_);
        // 更新邻居节点列表
        neighbor_addrs_ = std::move(new_neighbors);
    }

    for (const auto& node : neighbor_addrs_) {
        spdlog::info("Node {} added to neighbor_addrs_", node);
    }
    return grpc::Status::OK;
}

// 处理转发的网络度量请求
grpc::Status EdgeServer::ForwardNetworkMeasurement(grpc::ServerContext* context,
                                                 const cloud_edge_cache::ForwardNetworkMetricsRequest* request,
                                                 cloud_edge_cache::ForwardNetworkMetricsResponse* response) {
    const std::string& target_node = request->target_node();
    static constexpr int SAMPLE_SIZE = 1024 * 1024;  // 1MB 测试数据
    static constexpr int REPEAT_COUNT = 3;           // 测量次数
    static constexpr int COOLDOWN_MS = 100;         // 测量间隔时间
    
    double total_bandwidth = 0.0;
    double total_latency = 0.0;
    int successful_measurements = 0;
    
    // 创建到目标节点的通道
    auto channel = grpc::CreateChannel(target_node, grpc::InsecureChannelCredentials());
    auto stub = cloud_edge_cache::NetworkMetricsService::NewStub(channel);
    
    for (int i = 0; i < REPEAT_COUNT; ++i) {
        try {
            // 等待目标节点通道就绪
            if (!channel->WaitForConnected(gpr_time_add(
                    gpr_now(GPR_CLOCK_REALTIME),
                    gpr_time_from_seconds(5, GPR_TIMESPAN)))) {
                spdlog::error("Failed to connect to target node {}", target_node);
                return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Failed to connect to target node");
            }

            // 1. 测量基础延迟
            grpc::ClientContext base_context;
            cloud_edge_cache::ExecuteNetworkMetricsRequest base_request;
            cloud_edge_cache::ExecuteNetworkMetricsResponse base_response;
            auto start_base = std::chrono::high_resolution_clock::now();
            auto status = stub->ExecuteNetworkMeasurement(&base_context, base_request, &base_response);
            auto end_base = std::chrono::high_resolution_clock::now();
            
            if (!status.ok()) {
                throw std::runtime_error(status.error_message());
            }
            
            double base_latency = std::chrono::duration<double>(end_base - start_base).count();
            
            // std::cout << "Base latency: " << base_latency << std::endl;

            // 2. 测量总延迟 (包含传输延迟)
            std::this_thread::sleep_for(std::chrono::milliseconds(COOLDOWN_MS));
            
            grpc::ClientContext data_context;
            cloud_edge_cache::ExecuteNetworkMetricsRequest data_request;
            cloud_edge_cache::ExecuteNetworkMetricsResponse data_response;

            data_request.set_data(std::string(SAMPLE_SIZE, 'a'));
            auto start_total = std::chrono::high_resolution_clock::now();
            status = stub->ExecuteNetworkMeasurement(&data_context, data_request, &data_response);
            auto end_total = std::chrono::high_resolution_clock::now();
            if (!status.ok()) {
                throw std::runtime_error(status.error_message());
            }

            // 3. 计算各项指标
            double total_time = std::chrono::duration<double>(end_total - start_total).count();
            double transmission_delay = total_time - base_latency;
            double bandwidth = static_cast<double>(SAMPLE_SIZE) / (1024 * 1024 * transmission_delay); // MB/s
            
            // std::cout << "Bandwidth: " << bandwidth << " MB/s" << std::endl;

            // 4. 累加结果
            total_bandwidth += bandwidth;
            total_latency += base_latency;
            ++successful_measurements;
            // std::cout << "measure success to " << target_node << std::endl;
        } catch (const std::exception& e) {
            spdlog::error("Error in measurement to {}: {}", target_node, e.what());
        }
        
        // 测量间隔
        std::this_thread::sleep_for(std::chrono::milliseconds(COOLDOWN_MS));
    }
    
    // 设置响应结果
    if (successful_measurements > 0) {
        response->set_bandwidth(total_bandwidth / successful_measurements);
        response->set_latency(total_latency / successful_measurements);
        return grpc::Status::OK;
    } else {
        return grpc::Status(grpc::StatusCode::INTERNAL, 
                          "No successful measurements completed");
    }
}

// 直接执行网络度量请求
grpc::Status EdgeServer::ExecuteNetworkMeasurement(grpc::ServerContext* context,
                                                 const cloud_edge_cache::ExecuteNetworkMetricsRequest* request,
                                                 cloud_edge_cache::ExecuteNetworkMetricsResponse* response) {
    // 简单地返回接收到的数据，用于测量网络延迟和带宽
    response->set_data(request->data());
    return grpc::Status::OK;
}