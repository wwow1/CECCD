#include "center_server.h"

CenterServer::CenterServer() {
    auto& config = ConfigManager::getInstance();
    config.loadConfig("config/cluster_config.json");
    
    // 获取所有边缘节点地址
    for (const auto& node : config.getNodes()) {
        edge_server_addresses_.push_back(node.address);
        
        // 初始化节点容量
        NodeCapacity capacity;
        block_size_ = config.getBlockSizeMB();
        capacity.total_capacity = node.capacity_gb * 1024 / block_size_;  // 转换为字节
        capacity.used_capacity = 0;
        node_capacities_[node.address] = capacity;
    }
    
    prediction_period_ = std::chrono::milliseconds(
        config.getPredictionPeriodMs()
    );
    current_period_start_ = std::chrono::system_clock::now();
    
    center_addr_ = config.getCenterAddress();
    
    // 初始化网络度量
    initializeNetworkMetrics();
    
    // 启动预测循环
    std::thread(&CenterServer::cacheReplacementLoop, this).detach();
}

void CenterServer::initializeNetworkMetrics() {
    std::cout << "Initializing network metrics..." << std::endl;
    
    std::vector<std::string> all_nodes = edge_server_addresses_;
    all_nodes.push_back(center_addr_);  // 包含中心节点
    
    for (const auto& from_node : all_nodes) {
        for (const auto& to_node : all_nodes) {
            if (from_node != to_node) {
                measureNetworkMetrics(from_node, to_node);
            }
        }
    }
    
    std::cout << "Network metrics initialization completed" << std::endl;
}

void CenterServer::measureNetworkMetrics(const std::string& from_node, const std::string& to_node) {
    static constexpr int SAMPLE_SIZE = 1024 * 1024;  // 1MB 测试数据
    static constexpr int REPEAT_COUNT = 3;           // 测量次数
    static constexpr int COOLDOWN_MS = 100;         // 测量间隔时间
    
    double total_bandwidth = 0.0;
    double total_latency = 0.0;
    int successful_measurements = 0;
    
    // 创建 gRPC 通道和存根
    auto channel = grpc::CreateChannel(to_node, grpc::InsecureChannelCredentials());
    auto stub = cloud_edge_cache::NetworkMetricsService::NewStub(channel);
    
    for (int i = 0; i < REPEAT_COUNT; ++i) {
        grpc::ClientContext context;
        cloud_edge_cache::NetworkMetricsRequest request;
        cloud_edge_cache::NetworkMetricsResponse response;
        request.set_from_node(from_node);
        
        try {
            // 1. 测量基础延迟 (传播延迟 + 处理延迟)
            request.set_data("ping");
            auto start_base = std::chrono::high_resolution_clock::now();
            auto status = stub->MeasureNetwork(&context, request, &response);
            auto end_base = std::chrono::high_resolution_clock::now();
            
            if (!status.ok()) {
                throw std::runtime_error(status.error_message());
            }
            
            double base_latency = std::chrono::duration<double>(end_base - start_base).count();
            
            // 2. 测量总延迟 (包含传输延迟)
            grpc::ClientContext data_context;
            request.set_data(std::string(SAMPLE_SIZE, 'X'));
            auto start_total = std::chrono::high_resolution_clock::now();
            status = stub->MeasureNetwork(&data_context, request, &response);
            auto end_total = std::chrono::high_resolution_clock::now();
            
            if (!status.ok()) {
                throw std::runtime_error(status.error_message());
            }
            
            // 3. 计算各项指标
            double total_latency = std::chrono::duration<double>(end_total - start_total).count();
            double transmission_delay = total_latency - base_latency;
            double bandwidth = static_cast<double>(SAMPLE_SIZE) / (1024 * 1024 * transmission_delay); // MB/s
            
            // 4. 累加结果
            total_bandwidth += bandwidth;
            total_latency += total_latency;
            ++successful_measurements;
            
            // 输出每次测量的详细信息
            std::cout << "Measurement " << (i + 1) << " from " << from_node << " to " << to_node << ":\n"
                      << "  Base latency: " << base_latency * 1000 << " ms\n"
                      << "  Total latency: " << total_latency * 1000 << " ms\n"
                      << "  Transmission delay: " << transmission_delay * 1000 << " ms\n"
                      << "  Bandwidth: " << bandwidth << " MB/s\n" << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "Error in measurement " << (i + 1) << " between " << from_node 
                      << " and " << to_node << ": " << e.what() << std::endl;
        }
        
        // 测量间隔
        std::this_thread::sleep_for(std::chrono::milliseconds(COOLDOWN_MS));
    }
    
    // 存储最终结果
    if (successful_measurements > 0) {
        NetworkMetrics metrics;
        metrics.bandwidth = total_bandwidth / successful_measurements;
        metrics.latency = total_latency / successful_measurements;
        network_metrics_[from_node][to_node] = metrics;
        
        std::cout << "\nFinal network metrics from " << from_node << " to " << to_node << ":\n"
                  << "  Average bandwidth: " << metrics.bandwidth << " MB/s\n"
                  << "  Average latency: " << metrics.latency * 1000 << " ms\n"
                  << "  Successful measurements: " << successful_measurements 
                  << "/" << REPEAT_COUNT << std::endl;
    } else {
        std::cerr << "Warning: No successful measurements between " << from_node 
                  << " and " << to_node << std::endl;
    }
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
        
        std::cout << "Updated stats for block " << key 
                  << " from " << server_address
                  << " (selectivity=" << block_stat.selectivity() << ")" << std::endl;
    }
    
    return grpc::Status::OK;
}

void CenterServer::cacheReplacementLoop() {
    while (true) {
        std::this_thread::sleep_for(prediction_period_);
        
        // 1. 更新访问统计和预测
        auto period_stats = collectPeriodStats();
        updateAccessHistory(period_stats);
        
        // 2. 计算QCCV值并生成放置建议
        calculateQCCVs();
        auto new_allocation_plan = generateAllocationPlan();
        
        // 3. 执行缓存替换
        executeCacheReplacement(new_allocation_plan);
        
        std::cout << "Cache replacement loop iteration completed" << std::endl;
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

void CenterServer::calculateQCCVs() {
    replacement_queue_.clear();
    
    for (const auto& [key, history] : historical_stats_) {
        if (history.empty()) continue;
        
        // 计算加权访问统计
        auto weighted_stats = calculateWeightedStats(history);
        
        // 计算数据块在每个节点的QCCV值
        auto node_qccv_values = calculateNodeQCCVs(key, weighted_stats);
        
        // 将正QCCV值的节点添加到替换队列
        for (const auto& [node, qccv] : node_qccv_values) {
            replacement_queue_.addBlockQCCV(key, node, qccv);
        }
    }
    
    replacement_queue_.buildPriorityQueue();
}

std::unordered_map<std::string, std::unordered_set<std::string>> 
CenterServer::generateAllocationPlan() {
    std::unordered_map<std::string, std::unordered_set<std::string>> new_allocation_plan;
    int full_nodes = 0;
    
    while (auto placement = replacement_queue_.getTopPlacementChoice()) {
        if (full_nodes >= edge_server_addresses_.size()) break;
        
        auto [block_key, node] = placement.value();
        
        if (checkNodeCapacity(node)) {
            new_allocation_plan[node].insert(block_key);
            updateNodeCapacity(node);
        } else {
            replacement_queue_.addNextBestCandidate(block_key);
            full_nodes++;
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
    std::cerr << "Warning: No bandwidth data for " << from_node << " -> " << to_node 
              << ", using default value" << std::endl;
    return 100.0;  // 默认值
}

double CenterServer::getNetworkLatency(const std::string& from_node, const std::string& to_node) {
    if (network_metrics_.count(from_node) && network_metrics_[from_node].count(to_node)) {
        return network_metrics_[from_node][to_node].latency;
    }
    std::cerr << "Warning: No latency data for " << from_node << " -> " << to_node 
              << ", using default value" << std::endl;
    return 0.01;  // 默认值
}

// 实现 CacheReplacementQueue 的方法
void CenterServer::CacheReplacementQueue::addBlockQCCV(
    const std::string& block_key, 
    const std::string& node, 
    double qccv) {
    if (qccv > 0) {  // 只考虑正的QCCV值
        block_candidates_[block_key].emplace_back(node, qccv);
    }
}

void CenterServer::CacheReplacementQueue::addNextBestCandidate(const std::string& block_key) {
    auto& candidates = block_candidates_[block_key];
    if (!candidates.empty()) {
        // 找出最大QCCV值的候选节点
        auto max_candidate = std::max_element(
            candidates.begin(),
            candidates.end(),
            [](const auto& a, const auto& b) { return a.second < b.second; }
        );
        
        priority_queue_.push(std::make_tuple(
            max_candidate->second,
            block_key,
            max_candidate->first
        ));
    }
}

void CenterServer::CacheReplacementQueue::buildPriorityQueue() {
    // 清空现有队列
    while (!priority_queue_.empty()) {
        priority_queue_.pop();
    }
    
    // 对每个数据块，将其最大QCCV值的候选节点加入优先队列
    for (const auto& [block_key, candidates] : block_candidates_) {
        addNextBestCandidate(block_key);
    }
}

std::optional<std::pair<std::string, std::string>> 
CenterServer::CacheReplacementQueue::getTopPlacementChoice() {
    if (priority_queue_.empty()) {
        return std::nullopt;
    }
    
    auto [qccv, block_key, node] = priority_queue_.top();
    priority_queue_.pop();
    
    // 从该数据块的候选列表中移除已使用的选项
    auto& candidates = block_candidates_[block_key];
    candidates.erase(
        std::remove_if(candidates.begin(), candidates.end(),
            [&node](const auto& candidate) { return candidate.first == node; }
        ),
        candidates.end()
    );
    
    return std::make_pair(block_key, node);
}

bool CenterServer::checkNodeCapacity(
    const std::string& node) {
    if (node_capacities_.find(node) == node_capacities_.end()) {
        return false;
    }
    
    auto& node_capacity = node_capacities_[node];
    
    return (node_capacity.used_capacity + 1) <= node_capacity.total_capacity;
}

void CenterServer::updateNodeCapacity(
    const std::string& node) {
    if (node_capacities_.find(node) == node_capacities_.end()) {
        return;
    }
    auto& node_capacity = node_capacities_[node];
    node_capacity.used_capacity++;
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
    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(this); // Register both services

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    std::cout << "Center server is running on " << server_address << std::endl;
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
    for (const auto& [cache_node, stats] : weighted_stats) {
        double qccv = 0.0;
        double Pm_i_t = stats.first;   // 访问频率
        double Sm_i_t = stats.second;  // 选择率
        
        // 对每个可能的访问节点 n 计算传输时间差异
        for (const auto& [access_node, _] : weighted_stats) {
            // 计算查询结果大小 QD
            double QD_n_i_t = Sm_i_t * block_size_;  // QD = 选择率 * 原始数据块大小
            
            // 计算从缓存节点m到访问节点n的传输时间
            double W_mn = getNetworkBandwidth(cache_node, access_node);
            double T_pl_mn = getNetworkLatency(cache_node, access_node);
            double T_mn_i = QD_n_i_t / W_mn + T_pl_mn;
            
            // 计算从中心节点到访问节点n的传输时间
            double W_center_n = getNetworkBandwidth(center_addr_, access_node);
            double T_pl_center_n = getNetworkLatency(center_addr_, access_node);
            double T_0n_i = QD_n_i_t / W_center_n + T_pl_center_n;
            
            // 累加 QCCV
            double time_saving = (T_0n_i - T_mn_i);
            qccv += (time_saving * Pm_i_t) / block_size_;
        }
        
        node_qccv_values[cache_node] = qccv;
        std::cout << "QCCV for block " << block_key << " on node " << cache_node 
                  << ": " << qccv << std::endl;
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
    }
    
    // 使用新的索引添加相关的流元数据
    {
        std::lock_guard<std::mutex> lock(schema_mutex_);
        for (uint32_t unique_id : affected_streams) {
            auto it_schema = getStreamMeta(unique_id);
            auto* stream_meta = request.add_stream_metadata();
            stream_meta->set_datastream_id(it_schema.datastream_id_);
            stream_meta->set_unique_id(unique_id);
            stream_meta->set_start_time(it_schema.start_time_);
            stream_meta->set_time_range(it_schema.time_range_);
        }
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
        
        std::cout << "Successfully updated cache on " << edge_server_address 
                  << " (+" << blocks_to_add.size()
                  << "/-" << blocks_to_remove.size() 
                  << " blocks)" << std::endl;
    } else {
        std::cerr << "Failed to update cache on " << edge_server_address
                  << ": " << status.error_message() << std::endl;
    }
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <ip_address> <port>" << std::endl;
        return 1;
    }

    const std::string ip_address = argv[1];
    const std::string port = argv[2];
    const std::string server_address = ip_address + ":" + port;
    
    CenterServer center_server;
    center_server.Start(server_address);

    return 0;
}