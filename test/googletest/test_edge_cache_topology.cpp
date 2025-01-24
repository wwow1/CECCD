#include <gtest/gtest.h>
#include "edge_cache_index.h"
#include "config_manager.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <queue>
#include <tbb/concurrent_hash_map.h>
#include <random>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <numeric>
#include <algorithm>

class EdgeCacheTopologyTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 设置 spdlog
        auto file_logger = spdlog::basic_logger_mt("topology_test", "topology_test.log", true);  // true 表示截断已存在的文件
        spdlog::set_default_logger(file_logger);
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");  // 设置日志格式

        // 配置参数
        auto& config = ConfigManager::getInstance();
        config.loadConfig("config/cluster_config.json");
        
        // 初始化节点ID
        for (int i = 0; i < topu_radius; i++) {
            for (int j = 0; j < topu_radius; j++) {
                std::string node_id = "edge_" + std::to_string(i) + "_" + std::to_string(j);
                edge_nodes.push_back(node_id);
                
                // 为每个节点创建一个EdgeCacheIndex实例
                auto edge_index = std::make_unique<EdgeCacheIndex>();
                edge_index->setLatencyThreshold(1000); // 设置很高的阈值，确保使用布隆过滤器
                edge_indices[node_id] = std::move(edge_index);
            }
        }
        center_node = "center_0";
        
        // 设置节点间延迟
        setupNodeLatencies(topu_radius, hops_lat);
    }
    
    void TearDown() override {
        // 确保所有日志都被刷新到文件
        spdlog::shutdown();
    }
    
    // 修改延迟参数范围
    double min_hop_latency = 2.0;   // 最小跳延迟 2ms
    double max_hop_latency = 4.0;   // 最大跳延迟 4ms
    double min_center_latency = 10.0; // 最小中心延迟 10ms
    double max_center_latency = 20.0; // 最大中心延迟 20ms
    
    // 生成跳延迟的方法
    double generateHopLatency() {
        std::uniform_real_distribution<double> dist(min_hop_latency, max_hop_latency);
        return dist(rng);
    }
    
    // 生成中心节点延迟的方法
    double generateCenterLatency() {
        std::uniform_real_distribution<double> dist(min_center_latency, max_center_latency);
        return dist(rng);
    }
    
    void setupNodeLatencies(int topu_radius, int hops_lat) {
        // 设置边缘节点之间的延迟（topu_radius x topu_radius网格）
        for (int i = 0; i < topu_radius; i++) {
            for (int j = 0; j < topu_radius; j++) {
                std::string current = "edge_" + std::to_string(i) + "_" + std::to_string(j);
                auto& current_index = edge_indices[current];
                
                // 设置当前节点到中心节点的延迟（带抖动）
                current_index->setNodeLatency(center_node, generateCenterLatency());
                
                // 设置到所有其他边缘节点的延迟
                for (int ni = 0; ni < topu_radius; ni++) {
                    for (int nj = 0; nj < topu_radius; nj++) {
                        std::string target = "edge_" + std::to_string(ni) + "_" + std::to_string(nj);
                        if (i == ni && j == nj) {
                            current_index->setNodeLatency(target, 0);
                            continue;  // 跳过自身
                        }
                        // 计算曼哈顿距离作为跳数
                        int hops = std::abs(i - ni) + std::abs(j - nj);
                        // 为每一跳生成延迟并累加
                        double total_latency = 0.0;
                        for (int hop = 0; hop < hops; hop++) {
                            total_latency += generateHopLatency();
                        }
                        current_index->setNodeLatency(target, total_latency);
                    }
                }
            }
        }
    }
    // 添加 Zipf 分布生成器类
    class ZipfDistribution {
    private:
        double alpha;    // Zipf 分布的偏度参数
        double zeta;     // 归一化常数
        uint32_t n;      // 元素数量
        std::mt19937 rng;

        // 计算 Zeta 值用于归一化
        double calculateZeta() {
            double sum = 0.0;
            for (uint32_t i = 1; i <= n; i++) {
                sum += 1.0 / std::pow(i, alpha);
            }
            return sum;
        }

    public:
        ZipfDistribution(uint32_t n, double alpha = 0.8) 
            : n(n), alpha(alpha), rng(std::random_device{}()) {
            zeta = calculateZeta();
        }

        // 生成 Zipf 随机数
        uint32_t sample() {
            double u = std::uniform_real_distribution<>(0.0, 1.0)(rng);
            double sum = 0.0;
            for (uint32_t i = 1; i <= n; i++) {
                sum += 1.0 / (std::pow(i, alpha) * zeta);
                if (sum >= u) {
                    return i;
                }
            }
            return n;
        }

        // 获取特定排名的概率
        double getProbability(uint32_t rank) {
            return 1.0 / (std::pow(rank, alpha) * zeta);
        }
    };

    std::unordered_map<std::string, std::unique_ptr<EdgeCacheIndex>> edge_indices;
    std::vector<std::string> edge_nodes;
    std::string center_node;
    int topu_radius = 5;
    int hops_lat = 2;
    int index_constran_hop = 0;
    int center_lat = 10;
    int query_blocks = 3;
    uint32_t max_cache_block_num = 2048;
    uint32_t max_store_block_num = 1500;
    uint32_t max_stream_num = 250;
    uint32_t centarl_edge_node_access_frequency = 100000;     // 中心节点的基准访问频次
    double edge_node_decay_factor = 0.4;             // 边缘节点的访问频次衰减因子（相对于中心节点）
    
    ZipfDistribution block_zipf{max_store_block_num, 1.2};  // 块访问的 Zipf 分布
    // 添加用户到边缘服务器的延迟范围配置
    double min_user_latency = 2.0;  // 最小延迟 2ms
    double max_user_latency = 4.0;  // 最大延迟 4ms
    
    // 添加生成用户延迟的方法
    double generateUserLatency() {
        std::uniform_real_distribution<double> dist(min_user_latency, max_user_latency);
        return dist(rng);
    }

    uint32_t sampleStream(const std::pair<uint32_t, uint32_t>& preferred_range, uint32_t max_stream_num) {
        // 90% 概率选择本地流，10% 概率选择远程流
        std::bernoulli_distribution local_choice(0.95);
        
        if (local_choice(rng)) {
            // 从本地流范围中随机选择
            std::uniform_int_distribution<uint32_t> local_dist(
                preferred_range.first,
                preferred_range.second - 1
            );
            return local_dist(rng);
        } else {
            // 从远程流中随机选择
            std::uniform_int_distribution<uint32_t> remote_dist(0, max_stream_num - 1);
            uint32_t selected;
            do {
                selected = remote_dist(rng);
            } while (selected >= preferred_range.first && selected < preferred_range.second);
            return selected;
        }
    }
    // 添加缓存替换队列作为成员类
    class TestCacheReplacementQueue {
    public:
        using QueueElement = std::tuple<double, std::string, std::string>; // <QCCV值, 数据块ID, 节点ID>
        
        std::priority_queue<
            QueueElement,
            std::vector<QueueElement>,
            std::less<QueueElement>
        > priority_queue_;
        
        tbb::concurrent_hash_map<
            std::string, 
            std::vector<std::pair<std::string, double>>
        > block_candidates_;

        void addBlockQCCV(const std::string& block_key, 
                         const std::vector<std::pair<std::string, double>>& candidates) {
            block_candidates_.insert({block_key, candidates});
        }

        void addNextBestCandidate(const std::string& block_key) {
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
        
        void buildPriorityQueue() {
            tbb::concurrent_hash_map<std::string, std::vector<std::pair<std::string, double>>>::accessor accessor;
            for (auto it = block_candidates_.begin(); it != block_candidates_.end(); ++it) {
                const std::string& block_key = it->first;
                const auto& candidates = it->second;
                
                if (!candidates.empty()) {
                    auto best_candidate = *std::max_element(
                        candidates.begin(), candidates.end(),
                        [](const auto& a, const auto& b) { return a.second < b.second; }
                    );
                    priority_queue_.push(std::make_tuple(
                        best_candidate.second, block_key, best_candidate.first
                    ));
                }
            }
        }
        
        std::optional<std::pair<std::string, std::string>> getTopPlacementChoice() {
            if (priority_queue_.empty()) {
                return std::nullopt;
            }
            auto [qccv, block_key, node] = priority_queue_.top();
            priority_queue_.pop();

            // 使用 accessor 来访问 block_candidates_
            tbb::concurrent_hash_map<std::string, std::vector<std::pair<std::string, double>>>::accessor accessor;
            if (block_candidates_.find(accessor, block_key)) {
                auto& candidates = accessor->second;
                candidates.erase(
                    std::remove_if(candidates.begin(), candidates.end(),
                        [&node](const auto& candidate) { return candidate.first == node; }
                    ),
                    candidates.end()
                );
            }
            
            return std::make_pair(block_key, node);
        }
        
        void clear() {
            while (!priority_queue_.empty()) {
                priority_queue_.pop();
            }
            block_candidates_.clear();
        }
    };

    // 添加辅助方法
    std::unordered_map<std::string, std::unordered_set<std::string>> generateAllocationPlan(
        const std::set<std::string>& access_blocks,
        const std::unordered_map<std::string, std::unordered_map<std::string, uint32_t>>& access_records,
        const std::unordered_map<std::string, std::pair<uint32_t, uint32_t>>& node_stream_ranges,
        const std::unordered_map<std::string, uint32_t>& node_base_frequencies) {
        
        TestCacheReplacementQueue replacement_queue;

        // 计算每个数据块在每个节点上的QCCV值
        for (const auto& block_key : access_blocks) {
            std::vector<std::pair<std::string, double>> candidates;
            
            for (const auto& cache_node : edge_nodes) {
                // 计算QCCV值
                double qccv = 0.0;
                for (const auto& access_node : edge_nodes) {
                    if (access_records.find(access_node) == access_records.end()) {
                        spdlog::error("access_records.find(access_node) == access_records.end()");
                        continue;
                    }
                    if (access_records.at(access_node).count(block_key)) {
                        uint32_t access_count = access_records.at(access_node).at(block_key);
                        auto pos = block_key.find(':');
                        uint32_t stream_id = std::stoul(block_key.substr(0, pos));

                        double center_lat = access_count * edge_indices[access_node]->getNodeLatency(center_node);
                        double cache_lat = access_count * edge_indices[access_node]->getNodeLatency(cache_node);
                        qccv += access_count * (static_cast<double>(center_lat - cache_lat));
                    }
                }
                candidates.push_back({cache_node, qccv});
            }
            
            replacement_queue.addBlockQCCV(block_key, candidates);
        }

        // 构建优先队列
        replacement_queue.buildPriorityQueue();

        // 生成分配方案
        std::unordered_map<std::string, std::unordered_set<std::string>> allocation_plan;
        uint32_t total_capacity = max_cache_block_num;

        std::set<std::string> full_nodes;
        uint32_t full_nodes_num = 0;
        std::optional<std::pair<std::string, std::string>> placement;
        spdlog::info("priority_queue_size: {}", replacement_queue.priority_queue_.size());
        while ((placement = replacement_queue.getTopPlacementChoice()) && full_nodes_num < edge_nodes.size()) {
            const auto& [block_key, node] = placement.value();  // 使用 value() 来获取 optional 的值
            if (allocation_plan[node].size() < total_capacity) {
                allocation_plan[node].insert(block_key);
            } else {
                replacement_queue.addNextBestCandidate(block_key);
                if (full_nodes.find(node) == full_nodes.end()) {
                    full_nodes.insert(node);
                    full_nodes_num++;
                }
            }
        }

        return allocation_plan;
    }

    // 添加对照组的数据结构
    std::unordered_map<std::string, std::unordered_map<uint32_t, std::string>> reference_indices;
    
    // 添加获取哈希表内存使用的方法
    size_t getHashMapMemoryUsage(const std::string& node) {
        size_t total_size = 0;
        
        // 计算外层map的基本大小
        total_size += sizeof(std::unordered_map<uint32_t, std::string>);
        
        // 计算内部元素的大小
        if (reference_indices.find(node) != reference_indices.end()) {
            const auto& inner_map = reference_indices[node];
            // 每个bucket的大小
            total_size += inner_map.bucket_count() * sizeof(void*);
            
            // 每个元素的大小
            for (const auto& [block_id, target_node] : inner_map) {
                total_size += sizeof(uint32_t);  // key
                total_size += target_node.capacity() * sizeof(char);  // string value
                total_size += sizeof(void*) * 2; // 哈希表节点指针
            }
        }
        
        return total_size;
    }

    void executeAllocationPlan(
        const std::unordered_map<std::string, std::unordered_set<std::string>>& allocation_plan) {
        // 清理之前的索引
        reference_indices.clear();
        
        for (const auto& [node, blocks] : allocation_plan) {
            spdlog::info("executeAllocationPlan: node: {}, blocks: {}", node, blocks.size());
            for (const auto& block_key : blocks) {
                auto pos = block_key.find(':');
                uint32_t stream_id = std::stoul(block_key.substr(0, pos));
                uint32_t block_id = std::stoul(block_key.substr(pos + 1));
                
                // 更新布隆过滤器索引
                for (const auto& other_node : edge_nodes) {
                    edge_indices[other_node]->addBlock(block_id, node, stream_id);
                }
                
                // 更新哈希表索引（对照组）
                for (const auto& other_node : edge_nodes) {
                    reference_indices[other_node][block_id] = node;
                }
            }
        }

        // 输出内存使用对比
        for (const auto& node : edge_nodes) {
            for (const auto& other_node : edge_nodes) {
                if (node != other_node) {
                    size_t bloom_memory = edge_indices[node]->getSingleIndexMemoryUsage(other_node);
                    size_t hashmap_memory = getHashMapMemoryUsage(node);
                    
                    spdlog::info("Memory Usage Comparison - Node: {} -> {}", node, other_node);
                    spdlog::info("  Bloom Filter: {} bytes", bloom_memory);
                    spdlog::info("  Hash Map: {} bytes", hashmap_memory);
                    spdlog::info("  Memory Saving: {:.2f}%", 
                        100.0 * (hashmap_memory - bloom_memory) / hashmap_memory);
                }
            }
        }
    }
    // 在类成员变量中添加随机数生成器
    std::mt19937 rng{std::random_device{}()};
};

// 更新基本拓扑测试以适应新的延迟范围
TEST_F(EdgeCacheTopologyTest, BasicTopologySetup) {
    // 验证节点数量
    EXPECT_EQ(edge_nodes.size(), topu_radius * topu_radius);
    
    // 验证每个节点的延迟设置
    std::cout << "edge nodes: " << edge_nodes.size() << std::endl;
    for (const auto& source : edge_nodes) {
        auto& source_index = edge_indices[source];
        
        // 验证到中心节点的延迟在合理范围内
        double center_latency = source_index->getNodeLatency(center_node);
        EXPECT_GE(center_latency, min_center_latency);
        EXPECT_LE(center_latency, max_center_latency);
        
        // 验证到其他节点的延迟
        for (const auto& target : edge_nodes) {
            if (source != target) {
                int i1 = std::stoi(source.substr(5, 1));
                int j1 = std::stoi(source.substr(7, 1));
                int i2 = std::stoi(target.substr(5, 1));
                int j2 = std::stoi(target.substr(7, 1));
                
                // 计算曼哈顿距离（最短路径跳数）
                int manhattan_dist = std::abs(i1 - i2) + std::abs(j1 - j2);
                double latency = source_index->getNodeLatency(target);
                
                // 验证延迟在合理范围内
                EXPECT_GE(latency, manhattan_dist * min_hop_latency)
                    << "Latency too low from " << source << " to " << target;
                EXPECT_LE(latency, manhattan_dist * max_hop_latency)
                    << "Latency too high from " << source << " to " << target;
            }
        }
    }
}

TEST_F(EdgeCacheTopologyTest, ZipfDistributionAnalysis) {
    ZipfDistribution zipf(max_store_block_num, 1.0);
    
    // 计算前10个排名的概率
    std::cout << "Zipf distribution (alpha=0.4) probability analysis:" << std::endl;
    std::cout << "Rank\tProbability" << std::endl;
    double sum_prob = 0.0;
    for (int i = 1; i <= 10; i++) {
        double prob = zipf.getProbability(i);
        sum_prob += prob;
        std::cout << i << "\t" << prob << std::endl;
    }
    
    // 计算累积概率分布
    std::vector<int> ranges = {150, 200, 300, 100, max_store_block_num};
    double cumulative_prob = 0.0;
    std::cout << "\nCumulative probability distribution:" << std::endl;
    std::cout << "Range\tCumulative Probability" << std::endl;
    for (int range : ranges) {
        cumulative_prob = 0.0;
        for (int i = 1; i <= range; i++) {
            cumulative_prob += zipf.getProbability(i);
        }
        std::cout << "1-" << range << "\t" << cumulative_prob << std::endl;
    }
    
    // 进行采样测试
    const int sample_size = 100000;
    std::vector<int> sample_counts(max_store_block_num + 1, 0);
    
    for (int i = 0; i < sample_size; i++) {
        uint32_t sample = zipf.sample();
        sample_counts[sample]++;
    }
    
    // 输出采样结果的统计信息
    std::cout << "\nSampling test results (sample_size = {}):" << std::endl;
    std::cout << "Rank\tFrequency\tEmpirical Probability" << std::endl;
    for (int i = 1; i <= 10; i++) {
        double emp_prob = static_cast<double>(sample_counts[i]) / sample_size;
        std::cout << i << "\t" << sample_counts[i] << "\t" << emp_prob << std::endl;
    }
}


TEST_F(EdgeCacheTopologyTest, BlockAdditionAndQuery) {  
    // 为每个节点分配其偏好的数据流范围
    std::unordered_map<std::string, std::pair<uint32_t, uint32_t>> node_stream_ranges;
    uint32_t range_size = max_stream_num / (topu_radius * topu_radius);  // 每个节点偏好的流范围大小
    
    // 计算每个节点的基础访问频率（基于到中心的曼哈顿距离）
    std::unordered_map<std::string, uint32_t> node_base_frequencies;
    int center_i = topu_radius / 2;
    int center_j = topu_radius / 2;
    int max_manhattan_dist = topu_radius - 1;  // 最大曼哈顿距离就是从中心到边缘的距离
    spdlog::info("central_edge_node i: {}, j: {}", center_i, center_j);
    // 为每个节点计算基础访问频率
    for (const auto& node : edge_nodes) {
        int i = std::stoi(node.substr(5, 1));
        int j = std::stoi(node.substr(7, 1));
    
        // 计算到中心的曼哈顿距离
        int manhattan_dist = std::abs(i - center_i) + std::abs(j - center_j);
        double dist_ratio = static_cast<double>(manhattan_dist) / max_manhattan_dist;
        
        // 基础访问频率
        uint32_t base_freq = static_cast<uint32_t>(centarl_edge_node_access_frequency - centarl_edge_node_access_frequency * edge_node_decay_factor * dist_ratio);
        node_base_frequencies[node] = base_freq;
        
        // 分配数据流范围
        uint32_t start_stream = (i * topu_radius + j) * range_size;
        uint32_t end_stream = start_stream + range_size;
        node_stream_ranges[node] = {start_stream, end_stream};
        spdlog::info("node: {}, start_stream: {}, end_stream: {}, freq: {}", node, start_stream, end_stream, base_freq);
    }
    
    // 生成访问记录
    std::unordered_map<std::string, std::unordered_map<std::string, uint32_t>> 
        access_records; // node -> block_key -> access_count
    std::set<std::string> access_blocks_tran_set;

    // 第一轮：生成访问记录和分布
    for (const auto& [node, base_freq] : node_base_frequencies) {
        int i = std::stoi(node.substr(5, 1));
        int j = std::stoi(node.substr(7, 1));
        
        // 生成访问记录
        for (uint32_t query = 0; query < base_freq; query++) {
            uint32_t selected_stream = sampleStream(node_stream_ranges[node], max_stream_num);
            uint32_t selected_block = max_store_block_num - block_zipf.sample();
            while (selected_block < query_blocks) {
                selected_block = max_store_block_num - block_zipf.sample();
            }
            
            // 访问连续的query_blocks个块
            for (int i = 0; i < query_blocks; i++) {
                std::string block_key = std::to_string(selected_stream) + ":" + 
                                      std::to_string(selected_block - i);
                access_records[node][block_key] += 1;
                if (access_blocks_tran_set.find(block_key) == access_blocks_tran_set.end()) {
                    access_blocks_tran_set.insert(block_key);
                }
            }
        }
    }
    spdlog::info("access_blocks_tran_set: {}", access_blocks_tran_set.size());
    // 使用类成员方法生成和执行分配方案
    auto allocation_plan = generateAllocationPlan(access_blocks_tran_set, access_records, node_stream_ranges, node_base_frequencies);
    executeAllocationPlan(allocation_plan);

    // 第二轮：验证索引
    uint64_t total_queries = 0;
    uint64_t actual_false_positives = 0;
    uint64_t optimal_false_positives = 0;
    uint64_t not_found = 0;
    uint64_t find_count = 0;
    uint64_t local_fount = 0;
    std::set<std::string> access_blocks_test_set;

    // 添加两组延时统计相关变量
    std::vector<double> actual_latencies;    // 实际发生的延时
    std::vector<double> optimal_latencies;   // 理想情况下的延时
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> jitter_dist(1.0, 2.0);

    for (const auto& [node, base_freq] : node_base_frequencies) {
        spdlog::info("query phase: node: {}, base_freq: {}", node, base_freq);
        // 模拟查询
        for (uint32_t query = 0; query < base_freq; query++) {
            uint32_t selected_stream = sampleStream(node_stream_ranges[node], max_stream_num);
            uint32_t selected_block = max_store_block_num - block_zipf.sample();
            while (selected_block < query_blocks) {
                selected_block = max_store_block_num - block_zipf.sample();
            }
            
            // 为整个查询生成一个用户延迟（假设用户位置在查询期间不变）
            double user_latency = generateUserLatency();
            double max_actual_latency = user_latency;  // 初始化为用户延迟
            double max_optimal_latency = user_latency; // 初始化为用户延迟
            
            double jitter = jitter_dist(gen);
            total_queries++;
            bool ac_false_pos = false;
            bool op_false_pos = false;
            
            // 查询连续的query_blocks个块
            for (int i = 0; i < query_blocks; i++) {
                uint32_t current_block = selected_block - i;
                std::string block_key = std::to_string(selected_stream) + ":" + 
                                      std::to_string(current_block);
                
                if (access_blocks_test_set.find(block_key) == access_blocks_test_set.end()) {
                    access_blocks_test_set.insert(block_key);
                }

                auto results = edge_indices[node]->queryMainIndex(
                    std::to_string(selected_stream),
                    current_block,
                    current_block,
                    selected_stream
                );

                bool found_in_index = false;
                std::string found_node;
                
                typename tbb::concurrent_hash_map<uint32_t, std::string>::const_accessor accessor;
                if (results.find(accessor, current_block)) {
                    found_in_index = true;
                    found_node = accessor->second;
                }
                
                if (found_in_index) {
                    const auto& node_blocks = allocation_plan[found_node];
                    if (node_blocks.find(block_key) != node_blocks.end()) {
                        // 找到正确节点
                        find_count++;
                        double latency = std::min(edge_indices[node]->getNodeLatency(found_node), edge_indices[node]->getNodeLatency(center_node)) + user_latency;
                        if (found_node == node) {
                            local_fount++;
                            // spdlog::info("query found in loacl edge_server. lat:{}", latency);
                        }                                                                                                                 
                        max_actual_latency = std::max(max_actual_latency, latency);
                        max_optimal_latency = std::max(max_optimal_latency, latency);
                        continue;
                    }
                    if (!ac_false_pos) {
                        actual_false_positives++;
                        ac_false_pos = true;
                    }
                    
                    // 如果边缘侧的数据比中心节点还远，那不如直接访问中心
                    if (edge_indices[node]->getNodeLatency(found_node) > edge_indices[node]->getNodeLatency(center_node)) {
                        double actual_latency = user_latency + edge_indices[node]->getNodeLatency(center_node);
                        double optimal_latency = actual_latency;
                        max_actual_latency = std::max(max_actual_latency, actual_latency);
                        max_optimal_latency = std::max(max_optimal_latency, optimal_latency);
                    } else {
                        // 计算当前block的延迟（包含用户延迟）
                        double actual_latency = user_latency + 
                                            edge_indices[node]->getNodeLatency(found_node) +
                                            (edge_indices[node]->getNodeLatency(center_node) + 0.0);
                        max_actual_latency = std::max(max_actual_latency, actual_latency);
                        
                        // 如果超过跳数约束，计算理想情况的延迟
                        if (edge_indices[node]->getNodeLatency(found_node) >= index_constran_hop * hops_lat) {
                            double optimal_latency = user_latency + 
                                                edge_indices[node]->getNodeLatency(center_node);
                            max_optimal_latency = std::max(max_optimal_latency, optimal_latency);
                        } else {
                            max_optimal_latency = std::max(max_optimal_latency, actual_latency);
                            if (!op_false_pos) {
                                optimal_false_positives++;
                                op_false_pos = true;
                            }
                        }
                    }
                }
                not_found++;
            }
            
            // 只有当查询包含至少一个block时才记录延迟
            if (max_actual_latency > user_latency) {
                actual_latencies.push_back(max_actual_latency);
            }
            if (max_optimal_latency > user_latency) {
                optimal_latencies.push_back(max_optimal_latency);
            }
        }
    }
    uint64_t test_block_and_not_found_in_tran_set = 0;
    spdlog::info("access_blocks_test_set: {}", access_blocks_test_set.size());
    for (const auto& block_key : access_blocks_test_set) {
        if (access_blocks_tran_set.find(block_key) == access_blocks_tran_set.end()) {
            test_block_and_not_found_in_tran_set++;
        }
    }
    spdlog::info("test_block_and_not_found_in_tran_set: {}", test_block_and_not_found_in_tran_set);
    
    // 输出总体统计信息
    spdlog::info("Statistics: total_queries={}, not_found={}, find_count={}, actual_false_positives={}, optimal_false_positives={}, actual_false_positive_rate={:.2f}%, optimal_false_positive_rate={:.2f}%, find_rate={:.2f}%, local_fount={}", 
                 total_queries, not_found, find_count, actual_false_positives, optimal_false_positives, 
                 (double)actual_false_positives / total_queries * 100, (double)optimal_false_positives / total_queries * 100, (double)find_count / total_queries * 100, local_fount);
                 
    // 在测试结束时计算延时统计信息
    auto calculate_stats = [](const std::vector<double>& latencies, const std::string& type) {
        if (latencies.empty()) return;

        double avg_latency = std::accumulate(latencies.begin(), latencies.end(), 0.0) / latencies.size();

        std::vector<double> sorted_latencies = latencies;
        std::sort(sorted_latencies.begin(), sorted_latencies.end());
        size_t size = sorted_latencies.size();
        double p90 = sorted_latencies[static_cast<size_t>(size * 0.90)];
        double p95 = sorted_latencies[static_cast<size_t>(size * 0.95)];
        double p99 = sorted_latencies[static_cast<size_t>(size * 0.99)];

        spdlog::info("{} Latency Statistics:", type);
        spdlog::info("  Average: {:.2f}ms", avg_latency);
        spdlog::info("  P90: {:.2f}ms", p90);
        spdlog::info("  P95: {:.2f}ms", p95);
        spdlog::info("  P99: {:.2f}ms", p99);
    };

    calculate_stats(actual_latencies, "Actual");
    calculate_stats(optimal_latencies, "Optimal");
}