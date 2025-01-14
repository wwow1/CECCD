#include <gtest/gtest.h>
#include "edge_cache_index.h"
#include "config_manager.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <queue>
#include <tbb/concurrent_hash_map.h>
#include <random>

class EdgeCacheTopologyTest : public ::testing::Test {
protected:
    void SetUp() override {
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
    
    void setupNodeLatencies(int topu_radius, int hops_lat) {
        // 设置边缘节点之间的延迟（topu_radius x topu_radius网格）
        for (int i = 0; i < topu_radius; i++) {
            for (int j = 0; j < topu_radius; j++) {
                std::string current = "edge_" + std::to_string(i) + "_" + std::to_string(j);
                auto& current_index = edge_indices[current];
                
                // 设置当前节点到中心节点的延迟
                current_index->setNodeLatency(center_node, 60);
                
                // 设置到所有其他边缘节点的延迟
                for (int ni = 0; ni < topu_radius; ni++) {
                    for (int nj = 0; nj < topu_radius; nj++) {
                      std::string target = "edge_" + std::to_string(ni) + "_" + std::to_string(nj);
                        if (i == ni && j == nj) {
                          current_index->setNodeLatency(target, 0);
                        };  // 跳过自身
                        // 计算曼哈顿距离作为跳数
                        int hops = std::abs(i - ni) + std::abs(j - nj);
                        current_index->setNodeLatency(target, hops * hops_lat);
                    }
                }
            }
        }
    }

    std::unordered_map<std::string, std::unique_ptr<EdgeCacheIndex>> edge_indices;
    std::vector<std::string> edge_nodes;
    std::string center_node;
    int topu_radius = 4;
    int hops_lat = 5;
    int max_cache_block_num = 2048;
    int max_stream_num = 4000;

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

            return std::make_pair(block_key, node);
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
            }
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
                    if (access_records.at(access_node).count(block_key)) {
                        uint32_t access_count = access_records.at(access_node).at(block_key);
                        auto pos = block_key.find(':');
                        uint32_t stream_id = std::stoul(block_key.substr(0, pos));

                        double center_lat = base_freq * edge_indices[access_node]->getNodeLatency(center_node);
                        double cache_lat = base_freq * edge_indices[access_node]->getNodeLatency(cache_lat);
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
        while (auto placement = replacement_queue.getTopPlacementChoice() && full_nodes_num < edge_nodes.size()) {
            auto [block_key, node] = placement.value();
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

    void executeAllocationPlan(
        const std::unordered_map<std::string, std::unordered_set<std::string>>& allocation_plan) {
        for (const auto& [node, blocks] : allocation_plan) {
            for (const auto& block_key : blocks) {
                auto pos = block_key.find(':');
                uint32_t stream_id = std::stoul(block_key.substr(0, pos));
                uint32_t block_id = std::stoul(block_key.substr(pos + 1));
                for (const auto& other_node : edge_nodes) {
                    edge_indices[other_node]->addBlock(block_id, node, stream_id);
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

    ZipfDistribution block_zipf{max_cache_block_num, 0.8};  // 块访问的 Zipf 分布
    ZipfDistribution stream_zipf{max_stream_num, 0.6};      // 流访问的 Zipf 分布

    // 在类成员变量中添加随机数生成器
    std::mt19937 rng{std::random_device{}()};

    // 存储每个节点的数据流分布
    std::unordered_map<std::string, std::discrete_distribution<uint32_t>> node_stream_distributions;
};

// 测试基本的拓扑结构设置
TEST_F(EdgeCacheTopologyTest, BasicTopologySetup) {
    // 验证节点数量
    EXPECT_EQ(edge_nodes.size(), topu_radius * topu_radius);
    
    // 验证每个节点的延迟设置
    for (const auto& source : edge_nodes) {
        auto& source_index = edge_indices[source];
        
        // 验证到中心节点的延迟
        EXPECT_EQ(source_index->getNodeLatency(center_node), 60);
        
        // 验证到其他节点的延迟
        for (const auto& target : edge_nodes) {
            if (source != target) {
                int i1 = std::stoi(source.substr(5, 1));
                int j1 = std::stoi(source.substr(7, 1));
                int i2 = std::stoi(target.substr(5, 1));
                int j2 = std::stoi(target.substr(7, 1));
                
                // 计算曼哈顿距离（最短路径跳数）
                int manhattan_dist = std::abs(i1 - i2) + std::abs(j1 - j2);
                int expected_latency = manhattan_dist * hops_lat;
                
                EXPECT_EQ(source_index->getNodeLatency(target), expected_latency)
                    << "Unexpected latency from " << source << " to " << target
                    << " (expected " << expected_latency << "ms for "
                    << manhattan_dist << " hops)";
            }
        }
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
    
    // 为每个节点计算基础访问频率
    for (const auto& node : edge_nodes) {
        int i = std::stoi(node.substr(5, 1));
        int j = std::stoi(node.substr(7, 1));
        
        // 计算到中心的曼哈顿距离
        int manhattan_dist = std::abs(i - center_i) + std::abs(j - center_j);
        double dist_ratio = static_cast<double>(manhattan_dist) / max_manhattan_dist;
        
        // 基础访问频率：中心1000，最边缘600
        uint32_t base_freq = static_cast<uint32_t>(1000 - 400 * dist_ratio);
        node_base_frequencies[node] = base_freq;
        
        // 分配数据流范围
        uint32_t start_stream = (i * topu_radius + j) * range_size;
        uint32_t end_stream = start_stream + range_size;
        node_stream_ranges[node] = {start_stream, end_stream};
    }
    
    // 生成访问记录
    std::unordered_map<std::string, std::unordered_map<std::string, std::unordered_map<uint32_t, uint32_t>>> 
        access_records; // node -> block_key -> access_count
    std::set<std::string> access_blocks;

    // 第一轮：生成访问记录和分布
    for (const auto& [node, base_freq] : node_base_frequencies) {
        int i = std::stoi(node.substr(5, 1));
        int j = std::stoi(node.substr(7, 1));
        
        // 首先计算所有数据流的亲和度
        std::vector<double> stream_affinities(max_stream_num);
        
        // 计算每个数据流的亲和度，使用放大系数避免小数精度问题
        constexpr double SCALING_FACTOR = 1e6;  // 放大系数
        double max_affinity = 0.0;
        
        // 第一遍：计算亲和度并找出最大值
        for (uint32_t stream_id = 0; stream_id < max_stream_num; stream_id++) {
            uint32_t geo_rank;
            auto [preferred_start, preferred_end] = node_stream_ranges[node];
            
            if (stream_id >= preferred_start && stream_id < preferred_end) {
                // 本地数据流的排名在1-2之间浮动
                std::uniform_real_distribution<double> local_noise(1.0, 2.0);
                geo_rank = static_cast<uint32_t>(local_noise(rng));
            } else {
                // 找到这个数据流偏好的节点
                int preferred_i = (stream_id / range_size) / topu_radius;
                int preferred_j = (stream_id / range_size) % topu_radius;
                
                // 使用曼哈顿距离作为基础排名
                int manhattan_dist = std::abs(i - preferred_i) + std::abs(j - preferred_j);
                
                // 添加高斯噪声到距离计算中
                std::normal_distribution<double> dist_noise(manhattan_dist, 0.5);
                double noisy_dist = std::max(1.0, dist_noise(rng));
                
                geo_rank = static_cast<uint32_t>(noisy_dist) + 1;
            }
            
            // 为流的整体热度添加扰动
            std::normal_distribution<double> heat_noise(1.0, 0.1);
            stream_affinities[stream_id] = stream_zipf.getProbability(geo_rank) * heat_noise(rng);
            max_affinity = std::max(max_affinity, stream_affinities[stream_id]);
        }
        
        // 第二遍：相对于最大值进行放大，避免小数精度问题
        double total_scaled_affinity = 0.0;
        for (auto& affinity : stream_affinities) {
            affinity = (affinity / max_affinity) * SCALING_FACTOR;
            total_scaled_affinity += affinity;
        }
        
        // 使用放大后的值创建离散分布
        std::discrete_distribution<uint32_t> stream_distribution(
            stream_affinities.begin(), 
            stream_affinities.end()
        );
        
        // 保存该节点的数据流分布供后续使用
        node_stream_distributions[node] = stream_distribution;
        
        // 生成访问记录
        for (uint32_t query = 0; query < base_freq; query++) {
            uint32_t selected_stream = stream_distribution(rng);
            
            // 为选中的数据流生成块访问记录
            // 使用 Zipf 分布直接采样块（新块具有更高的访问概率）
            uint32_t selected_block = max_cache_block_num - 1 - block_zipf.sample(rng);
            std::string block_key = std::to_string(selected_stream) + ":" + std::to_string(selected_block);
            access_records[node][block_key]++;
            if (access_blocks.find(block_key) == access_blocks.end()) {
                access_blocks.insert(block_key);
            }
        }
    }
    
    // 使用类成员方法生成和执行分配方案
    auto allocation_plan = generateAllocationPlan(access_blocks, access_records, node_stream_ranges, node_base_frequencies);
    executeAllocationPlan(allocation_plan);

    // 第二轮：验证索引
    uint64_t total_queries = 0;
    uint64_t false_positives = 0;
    
    for (const auto& [node, base_freq] : node_base_frequencies) {
        // 直接使用保存的分布
        const auto& stream_distribution = node_stream_distributions[node];
        
        // 模拟查询
        for (uint32_t query = 0; query < base_freq; query++) {
            uint32_t selected_stream = stream_distribution(rng);
            uint32_t selected_block = block_zipf.sample(rng);
            total_queries++;
            
            // 查询本地索引中的指定块
            auto results = edge_indices[node]->queryMainIndex(
                std::to_string(selected_stream),
                selected_block,
                selected_block,
                selected_stream
            );
            // 检查是否在当前节点的索引中找到了该块
            bool found_in_index = false;
            std::string found_node;
            
            typename tbb::concurrent_hash_map<uint32_t, std::string>::const_accessor accessor;
            if (results.find(accessor, selected_block)) {
                found_in_index = true;
                found_node = accessor->second;
            }
            
            if (found_in_index) {
                // 检查实际分配计划
                for (const auto& alloc : allocation_plan[found_node]) {
                    if (alloc.find(selected_block) != alloc.end()) {
                        // 找到正确节点
                        break;
                    }
                    // 计算曼哈顿距离
                    int wrong_i = std::stoi(found_node.substr(5, 1));
                    int wrong_j = std::stoi(found_node.substr(7, 1));
                    int i = std::stoi(node.substr(5, 1));
                    int j = std::stoi(node.substr(7, 1));
                    int manhattan_dist = std::abs(i - wrong_i) + std::abs(j - wrong_j);
                    false_positives++;

                    std::cout << "False positive detected:\n"
                                << "  Querying node: " << node 
                                << "\n  Wrong node: " << found_node
                                << "\n  Manhattan distance: " << manhattan_dist
                                << "\n  Stream ID: " << selected_stream
                                << "\n  Block ID: " << selected_block 
                                << std::endl;
                }
            }
        }
    }
    
    // 输出总体统计信息
    std::cout << "Total queries: " << total_queries << std::endl;
    std::cout << "False positives: " << false_positives << std::endl;
    std::cout << "False positive rate: " 
              << (double)false_positives / total_queries * 100 
              << "%" << std::endl;
}