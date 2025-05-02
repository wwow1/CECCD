#include "center_server.h"
#include <vector>

CenterServer::CenterServer() {
    auto& config = ConfigManager::getInstance();
    
    // 创建控制台和文件 sink
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(config.getLogFilePath(), true);

    // 创建多重 sink logger
    auto logger = std::make_shared<spdlog::logger>("center_server_logger", spdlog::sinks_init_list{console_sink, file_sink});
    spdlog::set_default_logger(logger);

    // 设置日志级别
    std::string log_level = config.getCenterLogLevel();
    if (log_level == "debug") {
        spdlog::set_level(spdlog::level::debug);
    } else {
        spdlog::set_level(spdlog::level::info);
    }

    // 其他初始化代码
    block_size_ = config.getBlockSizeMB() * 1024 * 1024;
    center_addr_ = config.getCenterAddress();
    prediction_period_ = std::chrono::seconds(config.getPredictionPeriod());
    current_period_start_ = std::chrono::system_clock::now();

    std::string conn_str = config.getNodeDatabaseConfig(center_addr_).getConnectionString();
    try {
        spdlog::info("Initializing database connection pool: {}", conn_str);
        DBConnectionPool::getInstance().initialize(conn_str);
    } catch (const std::exception& e) {
        spdlog::error("Failed to initialize connection pool: {}", e.what());
        throw;
    }
    
    spdlog::info("CenterServer initialized with block size: {} bytes", block_size_);
}

grpc::Status CenterServer::Register(grpc::ServerContext* context,
                                    const cloud_edge_cache::RegisterRequest* request,
                                    cloud_edge_cache::Empty* response) {
    std::string node_addr = request->node_address();
    auto& config = ConfigManager::getInstance();
    
    {
        std::lock_guard<std::mutex> lock(nodes_mutex_);
        active_nodes_.insert(node_addr);
        edge_server_addresses_.push_back(node_addr);
        
        spdlog::info("New edge node registered: {}", node_addr);
    }
    initializeNetworkMetrics();
    notifyAllNodes(node_addr);
    
    return grpc::Status::OK;
}

void CenterServer::initializeNetworkMetrics() {
    spdlog::debug("Initializing network metrics...");
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    std::vector<std::string> all_nodes = edge_server_addresses_;
    all_nodes.push_back(center_addr_);

    for (uint32_t i = 0; i < all_nodes.size(); i++) {
        for (uint32_t j = i + 1; j < all_nodes.size(); j++) {
            auto& from_node = all_nodes[i];
            auto& to_node = all_nodes[j];
            spdlog::debug("Measuring network metrics from {} to {}", from_node, to_node);

            auto channel = grpc::CreateChannel(from_node, grpc::InsecureChannelCredentials());
            auto stub = cloud_edge_cache::NetworkMetricsService::NewStub(channel);
            
            if (!channel->WaitForConnected(gpr_time_add(
                    gpr_now(GPR_CLOCK_REALTIME),
                    gpr_time_from_seconds(5, GPR_TIMESPAN)))) {
                spdlog::error("Failed to connect to source node {}", from_node);
                return;
            }
            
            grpc::ClientContext context;
            cloud_edge_cache::ForwardNetworkMetricsRequest request;
            cloud_edge_cache::ForwardNetworkMetricsResponse response;
            request.set_from_node(from_node);
            request.set_target_node(to_node);
            stub->ForwardNetworkMeasurement(&context, request, &response);

            NetworkMetrics metrics;
            metrics.bandwidth = response.bandwidth();
            metrics.latency = response.latency();
            network_metrics_[from_node][to_node] = metrics;
            network_metrics_[to_node][from_node] = metrics;
            
            // spdlog::debug("Final network metrics from {} to {}: Average bandwidth: {} MB/s, Average latency: {} ms", 
            //              from_node, to_node, metrics.bandwidth, metrics.latency * 1000);
        }
    }
    
    spdlog::debug("Network metrics initialization completed");
}

grpc::Status CenterServer::ReportStatistics(grpc::ServerContext* context,
                                            const cloud_edge_cache::StatisticsReport* request,
                                            cloud_edge_cache::Empty* response) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    std::string server_address = request->server_address();
    
    for (const auto& block_stat : request->block_stats()) {
        // 这个唯一标识使用字符串合理吗？
        std::string key = std::to_string(block_stat.datastream_unique_id()) + ":" + 
                         std::to_string(block_stat.block_id());
        
        auto& stats = current_period_stats_[key];
        stats[server_address].first++;
        stats[server_address].second += block_stat.selectivity();
        
        spdlog::debug("Updated stats for block {} from {}", key, server_address);
    }
    
    return grpc::Status::OK;
}

void CenterServer::cacheReplacementLoop() {
    while (true) {
        std::this_thread::sleep_for(prediction_period_);
        
        spdlog::info("start Cache Replacement");
        // 1. 更新访问统计和预测
        auto period_stats = collectPeriodStats();
        updateAccessHistory(period_stats);
        
        // 2. 计算QCCV值并生成放置建议
        calculateQCCVs();
        auto new_allocation_plan = generateAllocationPlan();
        // 3. 执行缓存替换
        executeCacheReplacement(new_allocation_plan);
        
        spdlog::info("Cache replacement loop iteration completed");
    }
}

std::unordered_map<std::string, CenterServer::BlockStats> CenterServer::collectPeriodStats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    auto period_stats = std::move(current_period_stats_);
    current_period_stats_.clear();
    current_period_start_ = std::chrono::system_clock::now();
    return period_stats;
}

void CenterServer::updateAccessHistory(const std::unordered_map<std::string, BlockStats>& period_stats) {
    // 对每个数据块
    for (const auto& [key, stats] : period_stats) {
        // 将新的统计数据添加到历史记录末尾
        historical_stats_[key].push_back(stats);
        
        // 保持滑动窗口大小为5，如果超出则删除最旧的记录
        if (historical_stats_[key].size() > 5) {
            historical_stats_[key].erase(historical_stats_[key].begin());
        }
    }
}

void CenterServer::processQCCVTask(const std::string& key, const std::vector<BlockStats>& history) {
    auto weighted_stats = calculateWeightedStats(history);
    auto node_qccv_values = calculateNodeQCCVs(key, weighted_stats);
    std::vector<std::pair<std::string, double>> candidates;
    for (const auto& [node, qccv] : node_qccv_values) {
        candidates.push_back(std::make_pair(node, qccv));
        // std::cout << "block_key: " << key << "  node: " << node << "  qccv: " << qccv << std::endl;
    }
    replacement_queue_.addBlockQCCV(key, candidates);
}

void CenterServer::calculateQCCVs() {
    replacement_queue_.clear();
    ThreadPool pool(std::thread::hardware_concurrency());  // 创建线程池
    std::vector<std::future<void>> futures;  // 用于存储任务的 future 对象

    // std::cout << "historical_stats_ size: " << historical_stats_.size() << std::endl;
    for (const auto& [key, history] : historical_stats_) {
        if (history.empty()) continue;
        futures.push_back(pool.enqueue([this, key, history]() {
            processQCCVTask(key, history);
        }));
    }

    // 等待所有任务完成
    for (auto& future : futures) {
        future.get();
    }

    // // 添加调试日志
    // std::cout << "\nQCCV calculations completed. Block candidates:" << std::endl;
    // {
    //     tbb::concurrent_hash_map<std::string, std::vector<std::pair<std::string, double>>>::const_accessor accessor;
    //     for (auto it = replacement_queue_.block_candidates_.begin(); 
    //          it != replacement_queue_.block_candidates_.end(); ++it) {
    //         std::cout << "Block " << it->first << " candidates count: " << it->second.size() << std::endl;
    //     }
    // }

    replacement_queue_.buildPriorityQueue();
}

std::unordered_map<std::string, std::unordered_set<std::string>> 
CenterServer::generateAllocationPlan() {
    std::unordered_map<std::string, std::unordered_set<std::string>> new_allocation_plan;
    auto& config = ConfigManager::getInstance();
    uint32_t total_capacity = config.getEdgeCapacityGB() * 1024 / config.getBlockSizeMB();
    std::unordered_map<std::string, uint32_t> used_node_capacities;
    std::unordered_set<std::string> full_nodes;
    uint32_t full_nodes_num = 0;
    for (const auto& node : edge_server_addresses_) {
        used_node_capacities[node] = 0;
    }

    while (auto placement = replacement_queue_.getTopPlacementChoice()) {
        if (full_nodes_num >= edge_server_addresses_.size()) break;
        
        auto [block_key, node] = placement.value();

        if ((used_node_capacities[node] + 1) <= total_capacity) {
            new_allocation_plan[node].insert(block_key);
            // std::cout << "Allocated block " << block_key 
            //          << " to node " << node 
            //          << " (used: " << used_node_capacities[node] 
            //          << "/" << total_capacity << ")" << std::endl;
            used_node_capacities[node]++;
        } else {
            replacement_queue_.addNextBestCandidate(block_key);
            spdlog::debug("Node {} is full, adding block {} to replacement queue", node, block_key);
            if (full_nodes.find(node) == full_nodes.end()) {
                full_nodes.insert(node);
                full_nodes_num++;
            }
        }
    }
    
    return new_allocation_plan;
}

void CenterServer::executeCacheReplacement(
    const std::unordered_map<std::string, std::unordered_set<std::string>>& new_allocation_plan) {
    
    // 计算增量变化
    auto [blocks_to_add, blocks_to_remove] = calculateIncrementalChanges(new_allocation_plan);
    
    // 执行实际的缓存替换
    for (const auto& edge_server_address : edge_server_addresses_) {
        if (blocks_to_add[edge_server_address].empty() && 
            blocks_to_remove[edge_server_address].empty()) {
            continue;
        }
        spdlog::info("Executing cache replacement for node {}: add blocks:{}, remove blocks:{}", 
            edge_server_address, blocks_to_add[edge_server_address].size(), blocks_to_remove[edge_server_address].size());
        executeNodeCacheUpdate(
            edge_server_address, 
            blocks_to_add[edge_server_address], 
            blocks_to_remove[edge_server_address]
        );
    }
}

// 修改现有的获取网络指标的方法
double CenterServer::getNetworkBandwidth(const std::string& from_node, const std::string& to_node) {
    if (network_metrics_.count(from_node) && network_metrics_[from_node].count(to_node)) {
        return network_metrics_[from_node][to_node].bandwidth;
    }
    spdlog::error("Warning: No bandwidth data for {} -> {}, using default value", from_node, to_node);
    return 100.0;  // 默认值
}

double CenterServer::getNetworkLatency(const std::string& from_node, const std::string& to_node) {
    if (network_metrics_.count(from_node) && network_metrics_[from_node].count(to_node)) {
        return network_metrics_[from_node][to_node].latency;
    }
    spdlog::error("Warning: No latency data for {} -> {}, using default value", from_node, to_node);
    return 0.01;  // 默认值
}

// 实现 CacheReplacementQueue 的方法
void CenterServer::CacheReplacementQueue::addBlockQCCV(
    const std::string& block_key, 
    const std::vector<std::pair<std::string, double>>& candidates) { 
    tbb::concurrent_hash_map<std::string, std::vector<std::pair<std::string, double>>>::accessor accessor;
    if (!block_candidates_.find(accessor, block_key)) {
        // 如果键不存在，创建新条目
        block_candidates_.insert(accessor, block_key);
    }
    // 将 candidates 中的所有元素添加到 vector 中
    accessor->second = candidates;  // 直接赋值替换，因为这是一个完整的候选列表
    
    // // 添加调试日志
    // std::cout << "Added candidates for block " << block_key << ":" << std::endl;
    // for (const auto& [node, qccv] : candidates) {
    //     std::cout << "  Node: " << node << ", QCCV: " << qccv << std::endl;
    // }
}

void CenterServer::CacheReplacementQueue::addNextBestCandidate(const std::string& block_key) {
    tbb::concurrent_hash_map<std::string, std::vector<std::pair<std::string, double>>>::accessor accessor;
    if (block_candidates_.find(accessor, block_key)) {
        auto& candidates = accessor->second;
        if (!candidates.empty()) {
            // 找出最大QCCV值的候选节点
            auto max_candidate = std::max_element(
                candidates.begin(),
                candidates.end(),
                [](const auto& a, const auto& b) { return a.second < b.second; }
            );
            // std::cout << "max_candidate: " << max_candidate->first << "  qccv: " << max_candidate->second << std::endl;
            priority_queue_.push(std::make_tuple(
                max_candidate->second,
                block_key,
                max_candidate->first
            ));
        }
    }
}

void CenterServer::CacheReplacementQueue::buildPriorityQueue() {
    // 清空现有队列
    while (!priority_queue_.empty()) {
        priority_queue_.pop();
    }
    
    spdlog::debug("\nBuilding priority queue with candidates:");
    
    // 首先收集所有的 block keys（workaround）
    std::vector<std::string> block_keys;
    {
        tbb::concurrent_hash_map<std::string, std::vector<std::pair<std::string, double>>>::const_accessor accessor;
        for (auto it = block_candidates_.begin(); it != block_candidates_.end(); ++it) {
            block_keys.push_back(it->first);
            spdlog::debug("Found block {} with {} candidates", it->first, it->second.size());
        }
    }
    
    // 然后处理每个 block
    for (const auto& block_key : block_keys) {
        addNextBestCandidate(block_key);
    }
    
    // 打印最终队列内容
    spdlog::debug("\nFinal priority queue contents:");
    auto temp_queue = priority_queue_;
    while (!temp_queue.empty()) {
        auto [qccv, block_key, node] = temp_queue.top();
        // spdlog::debug("QCCV: {}, Block: {}, Node: {}", qccv, block_key, node);
        spdlog::debug("Block: {}, Node: {}", block_key, node);
        temp_queue.pop();
    }
}

std::optional<std::pair<std::string, std::string>> 
CenterServer::CacheReplacementQueue::getTopPlacementChoice() {
    if (priority_queue_.empty()) {
        spdlog::info("Priority queue is empty");
        return std::nullopt;
    }
    
    auto [qccv, block_key, node] = priority_queue_.top();
    priority_queue_.pop();
    
    spdlog::debug("Getting top placement choice: Block {} -> Node {}", block_key, node);
    
    // 使用 accessor 来访问 block_candidates_
    tbb::concurrent_hash_map<std::string, std::vector<std::pair<std::string, double>>>::accessor accessor;
    if (block_candidates_.find(accessor, block_key)) {
        auto& candidates = accessor->second;
        auto before_size = candidates.size();
        candidates.erase(
            std::remove_if(candidates.begin(), candidates.end(),
                [&node](const auto& candidate) { return candidate.first == node; }
            ),
            candidates.end()
        );
        // spdlog::debug("Removed candidate for block {}, (candidates before: {}, after: {})", block_key, before_size, candidates.size());
    }
    
    return std::make_pair(block_key, node);
}

void CenterServer::updateSchema(const std::string& datastream_id, const Common::StreamMeta& meta) {
    std::lock_guard<std::mutex> lock(schema_mutex_);
    schema_[datastream_id] = meta;
    unique_id_to_datastream_id_[meta.unique_id_] = datastream_id;
}

Common::StreamMeta CenterServer::getStreamMeta(const std::string& stream_id) {
    std::lock_guard<std::mutex> lock(schema_mutex_);
    auto it = schema_.find(stream_id);
    if (it == schema_.end()) {
        throw std::runtime_error("Stream metadata not found: " + stream_id);
    }
    return it->second;
}

Common::StreamMeta CenterServer::getStreamMeta(const uint32_t unique_id) {
    std::lock_guard<std::mutex> lock(schema_mutex_);
    auto it = unique_id_to_datastream_id_.find(unique_id);
    if (it == unique_id_to_datastream_id_.end()) {
        throw std::runtime_error("Stream metadata not found: " + std::to_string(unique_id));
    }
    return getStreamMeta(it->second);
}

void CenterServer::Start(const std::string& server_address) {
    // 初始化 schema
    initializeSchema();
    
    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    
    // 注册所有服务时添加新服务
    builder.RegisterService(static_cast<cloud_edge_cache::ClientToCenter::Service*>(this));
    
    // 注册所有服务
    builder.RegisterService(static_cast<cloud_edge_cache::EdgeToCenter::Service*>(this));
    builder.RegisterService(static_cast<cloud_edge_cache::CenterToEdge::Service*>(this));
    builder.RegisterService(static_cast<cloud_edge_cache::NetworkMetricsService::Service*>(this));
    builder.RegisterService(static_cast<cloud_edge_cache::EdgeToEdge::Service*>(this));
    
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    spdlog::info("Center server is running on {}", server_address);
    
    // 启动预测循环
    std::thread(&CenterServer::cacheReplacementLoop, this).detach();
    
    server->Wait();
}

CenterServer::WeightedStats CenterServer::calculateWeightedStats(const std::vector<BlockStats>& history) {
    WeightedStats node_weighted_sums;
    std::unordered_map<std::string, double> node_weight_sums;
    
    const double decay_factor = 0.7; // 衰减因子
    double weight = 1.0;
    
    // 从最近的记录开始计算
    for (auto it = history.rbegin(); it != history.rend(); ++it) {
        const auto& period_stats = *it;
        for (const auto& [node, stats] : period_stats) {
            // stats.first 是访问次数，stats.second 是选择率
            node_weighted_sums[node].first += stats.first * weight;
            node_weighted_sums[node].second += stats.second * weight;
            node_weight_sums[node] += weight;
        }
        weight *= decay_factor;
    }

    // 计算加权平均值
    WeightedStats result;
    for (const auto& [node, sums] : node_weighted_sums) {
        double weight_sum = node_weight_sums[node];
        result[node] = {
            sums.first / weight_sum,  // 平均访问频率
            sums.second / weight_sum  // 平均选择率
        };
    }
    
    return result;
}

std::unordered_map<std::string, double> 
CenterServer::calculateNodeQCCVs(const std::string& block_key, const WeightedStats& weighted_stats) {
    std::unordered_map<std::string, double> node_qccv_values;

    // 对每个可能的缓存节点 m 计算 QCCV
    for (const auto& cache_node : edge_server_addresses_) {
        double qccv = 0.0;
        
        // 对每个可能的访问节点 n 计算传输时间差异
        for (const auto& [access_node, stats] : weighted_stats) {
            double Pm_i_t = stats.first;   // 访问频率
            double Sm_i_t = stats.second;  // 选择率
            double QD_n_i_t = Sm_i_t * block_size_;  // QD = 选择率 * 原始数据块大小

            // 计算传输时间
            double T_mn_i = 0;
            if (cache_node != access_node) {
                double W_mn = getNetworkBandwidth(cache_node, access_node);
                double T_pl_mn = getNetworkLatency(cache_node, access_node);
                T_mn_i = QD_n_i_t / W_mn + T_pl_mn;
            }
            
            double W_center_n = getNetworkBandwidth(center_addr_, access_node);
            double T_pl_center_n = getNetworkLatency(center_addr_, access_node);
            double T_0n_i = QD_n_i_t / W_center_n + T_pl_center_n;
            
            double time_saving = (T_0n_i - T_mn_i);
            qccv += (time_saving * Pm_i_t) / block_size_;
        }
        
        node_qccv_values[cache_node] = qccv;
        // std::cout << "block_key: " << block_key << "  Final QCCV for node " << cache_node << ": " << qccv << std::endl;
    }
    
    return node_qccv_values;
}

std::pair<
    std::unordered_map<std::string, std::vector<std::string>>,
    std::unordered_map<std::string, std::vector<std::string>>
>
CenterServer::calculateIncrementalChanges(
    const std::unordered_map<std::string, std::unordered_set<std::string>>& new_allocation_plan) {
    
    std::unordered_map<std::string, std::vector<std::string>> blocks_to_add;
    std::unordered_map<std::string, std::vector<std::string>> blocks_to_remove;

    // 计算每个节点的增量变化
    for (const auto& [node, new_blocks] : new_allocation_plan) {
        const auto& current_blocks = node_block_allocation_map_[node];
        
        // 找出需要新增的块
        for (const auto& block : new_blocks) {
            if (current_blocks.find(block) == current_blocks.end()) {
                blocks_to_add[node].push_back(block);
            }
        }
        
        // 找出需要删除的块
        for (const auto& block : current_blocks) {
            if (new_blocks.find(block) == new_blocks.end()) {
                blocks_to_remove[node].push_back(block);
            }
        }
    }
    
    return {blocks_to_add, blocks_to_remove};
}

void CenterServer::executeNodeCacheUpdate(
    const std::string& edge_server_address,
    const std::vector<std::string>& blocks_to_add,
    const std::vector<std::string>& blocks_to_remove) {
    
    auto stub = cloud_edge_cache::CenterToEdge::NewStub(
        grpc::CreateChannel(edge_server_address, grpc::InsecureChannelCredentials())
    );

    cloud_edge_cache::CacheReplacement request;
    std::unordered_set<uint32_t> affected_streams;
    
    // 处理需要添加的块
    for (const auto& block_key : blocks_to_add) {
        // 解析 block_key (格式: "stream_id:block_id")
        auto pos = block_key.find(':');
        if (pos == std::string::npos) continue;
        
        uint32_t stream_id = std::stoul(block_key.substr(0, pos));
        uint32_t block_id = std::stoul(block_key.substr(pos + 1));
        
        auto* block_op = request.add_block_operations();
        block_op->set_datastream_unique_id(stream_id);
        block_op->set_block_id(block_id);
        block_op->set_operation(cloud_edge_cache::BlockOperation::ADD);
        
        affected_streams.insert(stream_id);
        spdlog::debug("executeNodeCacheUpdate:add block {} to node {}", block_key, edge_server_address);
        // std::cout << "executeNodeCacheUpdate:add block " << block_key << " to node " << edge_server_address << std::endl;
    }
    
    // 处理需要删除的块
    for (const auto& block_key : blocks_to_remove) {
        auto pos = block_key.find(':');
        if (pos == std::string::npos) continue;
        
        uint32_t stream_id = std::stoul(block_key.substr(0, pos));
        uint32_t block_id = std::stoul(block_key.substr(pos + 1));
        
        auto* block_op = request.add_block_operations();
        block_op->set_datastream_unique_id(stream_id);
        block_op->set_block_id(block_id);
        block_op->set_operation(cloud_edge_cache::BlockOperation::REMOVE);
        spdlog::debug("executeNodeCacheUpdate:remove block {} from node {}", block_key, edge_server_address);
        // std::cout << "executeNodeCacheUpdate:remove block " << block_key << " from node " << edge_server_address << std::endl;
    }

    // 执行远程调用
    cloud_edge_cache::Empty response;
    grpc::ClientContext context;
    grpc::Status status = stub->ReplaceCache(&context, request, &response);

    if (status.ok()) {
        // 更新本地索引
        for (const auto& block : blocks_to_add) {
            node_block_allocation_map_[edge_server_address].insert(block);
        }
        for (const auto& block : blocks_to_remove) {
            node_block_allocation_map_[edge_server_address].erase(block);
        }
        
        spdlog::info("Successfully updated cache on {}, (+{}/-{} blocks)", edge_server_address, blocks_to_add.size(), blocks_to_remove.size());
    } else {
        spdlog::error("Failed to update cache on {}, {}", edge_server_address, status.error_message());
    }
}

uint64_t CenterServer::calculateTimeRange(const std::string& table_name, size_t rows_per_block, pqxx::work& txn) {
    // 使用 CAST 将 date_time 转换为 bigint
    pqxx::result time_diff = txn.exec(
        "SELECT CAST(date_time AS bigint) AS date_time "
        "FROM " + table_name + " "
        "ORDER BY date_time LIMIT 2"
    );
    
    uint64_t time_difference = time_diff[1]["date_time"].as<uint64_t>() - 
                              time_diff[0]["date_time"].as<uint64_t>();
    //spdlog::info("time_difference: {} rows_per_block: {}", time_difference, rows_per_block);
    return time_difference * rows_per_block;
}

void CenterServer::initializeSchema() {
    auto& db_config = ConfigManager::getInstance().getDatabaseConfig();
    spdlog::info("Database config: {}", db_config.getConnectionString());
    
    auto conn = DBConnectionPool::getInstance().getConnection();
    try {
        pqxx::work txn(*conn);
        // 获取所有表名
        pqxx::result tables = txn.exec("SELECT table_name FROM information_schema.tables WHERE table_schema='public'");

        for (const auto& row : tables) {
            std::string table_name = row["table_name"].c_str();
            uint32_t unique_id = generateUniqueId(table_name);

            // 使用 ROUND 函数将浮点数四舍五入为整数
            pqxx::result avg_size = txn.exec(
                "SELECT ROUND(AVG(pg_column_size(t.*))) as avg_row_size "
                "FROM " + table_name + " t "
                "TABLESAMPLE SYSTEM(1)"  // 采样1%的数据
            );
            size_t avg_row_size = avg_size[0]["avg_row_size"].as<size_t>();

            // 获取表中最小的 date_time，使用 CAST 转换为 bigint
            pqxx::result min_time = txn.exec(
                "SELECT CAST(MIN(date_time) AS bigint) as min_time FROM " + table_name
            );
            uint64_t start_time = min_time[0]["min_time"].as<uint64_t>();

            size_t rows_per_block = block_size_ / avg_row_size;
            uint64_t time_range = calculateTimeRange(table_name, rows_per_block, txn);

            Common::StreamMeta meta;
            meta.datastream_id_ = table_name;
            meta.unique_id_ = unique_id;
            meta.start_time_ = start_time;
            meta.time_range_ = time_range;
            updateSchema(table_name, meta);
            
            // spdlog::info("Updated schema for table {}, unique_id {}, start_time {}, time_range {}, avg_row_size {}, rows_per_block {}", table_name, unique_id, start_time, time_range, avg_row_size, rows_per_block);
            spdlog::info("Updated schema for table {}, unique_id {}", table_name, unique_id);
        }
        DBConnectionPool::getInstance().returnConnection(conn);
    } catch (const std::exception& e) {
        DBConnectionPool::getInstance().returnConnection(conn);
        spdlog::error("Error initializing schema: {}", e.what());
        throw;
    }
}

uint32_t CenterServer::generateUniqueId(const std::string& table_name) {
    static uint32_t last_id = 0;  // Static variable to keep track of the last assigned ID
    return last_id++;  // Increment and return the last assigned ID
}

void CenterServer::notifyAllNodes(const std::string& new_node) {
    cloud_edge_cache::ClusterNodesUpdate update;
    {
        std::lock_guard<std::mutex> lock(nodes_mutex_);
        for (const auto& node : active_nodes_) {
            auto* node_info = update.add_nodes();
            node_info->set_node_address(node);
        }
    }
    {
        // 传输所有数据流信息
        std::lock_guard<std::mutex> lock(schema_mutex_);
        // 遍历所有 schema 数据
        for (const auto& [stream_id, meta] : schema_) {
            spdlog::info("notify schema stream_id {}", stream_id);
            auto* stream_meta = update.add_stream_metadata();
            stream_meta->set_datastream_id(meta.datastream_id_);
            stream_meta->set_unique_id(meta.unique_id_);
            stream_meta->set_start_time(meta.start_time_);
            stream_meta->set_time_range(meta.time_range_);
        }
    }
    
    // 通知所有节点（包括新节点）
    for (const auto& node : active_nodes_) {
        auto channel = grpc::CreateChannel(node, grpc::InsecureChannelCredentials());
        auto stub = cloud_edge_cache::CenterToEdge::NewStub(channel);
        
        grpc::ClientContext context;
        cloud_edge_cache::Empty response;
        
        auto status = stub->UpdateClusterNodes(&context, update, &response);
        
        if (!status.ok()) {
            spdlog::error("Failed to notify node {}, {}", node, status.error_message());
        } else {
            spdlog::info("Successfully notified node {} of cluster update", node);
        }
    }
}


// 处理转发的网络度量请求
grpc::Status CenterServer::ForwardNetworkMeasurement(grpc::ServerContext* context,
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
            
            // 4. 累加结果
            total_bandwidth += bandwidth;
            total_latency += base_latency;
            ++successful_measurements;
            
        } catch (const std::exception& e) {
            spdlog::error("Error in measurement to {}, {}", target_node, e.what());
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
grpc::Status CenterServer::ExecuteNetworkMeasurement(grpc::ServerContext* context,
                                                 const cloud_edge_cache::ExecuteNetworkMetricsRequest* request,
                                                 cloud_edge_cache::ExecuteNetworkMetricsResponse* response) {
    // 简单地返回接收到的数据，用于测量网络延迟和带宽
    response->set_data(request->data());
    return grpc::Status::OK;
}

grpc::Status CenterServer::SubQuery(grpc::ServerContext* context,
                                const cloud_edge_cache::QueryRequest* request,
                                cloud_edge_cache::SubQueryResponse* response) {
    // std::cout << "Received SubQuery request to BlockId{} " << request->sql_query() << std::endl;
    spdlog::info("Received SubQuery request to BlockId<{}:{}> ", request->stream_unique_id(), request->block_id());
    auto conn = DBConnectionPool::getInstance().getConnection();
    try {
        pqxx::work txn(*conn);
        pqxx::result db_result = txn.exec(request->sql_query());
        txn.commit();

        // 设置响应状态
        response->set_status(cloud_edge_cache::SubQueryResponse::OK);
        
        // 填充查询结果
        auto* query_response = response->mutable_result();
        
        // 设置列信息
        if (!db_result.empty()) {
            // 遍历列
            for (int i = 0; i < db_result.columns(); ++i) {
                auto* col = query_response->add_columns();
                col->set_name(db_result.column_name(i));
                
                // 将 PostgreSQL OID 转换为字符串
                pqxx::oid type_oid = db_result.column_type(i);
                std::string type_str = std::to_string(type_oid);  // 或者使用 OID 到类型名称的映射
                col->set_type(type_str);
            }

            // 添加行数据
            for (const auto& db_row : db_result) {
                auto* row = query_response->add_rows();
                for (size_t i = 0; i < db_row.size(); ++i) {
                    row->add_values(db_row[i].is_null() ? "" : db_row[i].c_str());
                }
            }
        }
        DBConnectionPool::getInstance().returnConnection(conn);
        return grpc::Status::OK;
    } catch (const std::exception& e) {
        spdlog::error("Error in SubQuery: {}", e.what());
        response->set_status(cloud_edge_cache::SubQueryResponse::ERROR);
        response->set_error_message(e.what());
        DBConnectionPool::getInstance().returnConnection(conn);
        return grpc::Status(grpc::StatusCode::INTERNAL, e.what());
    }
}
// 在CenterServer类中添加新方法实现
grpc::Status CenterServer::GetEdgeNodes(grpc::ServerContext* context,
                                      const cloud_edge_cache::Empty* request,
                                      cloud_edge_cache::EdgeNodesInfo* response) {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    for (const auto& node : edge_server_addresses_) {
        response->add_edge_nodes(node);
    }
    return grpc::Status::OK;
}