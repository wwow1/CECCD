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

// 将 CacheStrategy 枚举移到类外部
enum CacheStrategy { LECS, TRECS }; // 前向声明

class EdgeCacheTRECSTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 设置 spdlog
        auto file_logger = spdlog::basic_logger_mt("trecs_test", "trecs__test.log", true);  // true 表示截断已存在的文件
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
                edge_index->setLatencyThreshold(index_constran_lat); // 设置很高的阈值，确保使用布隆过滤器
                edge_indices[node_id] = std::move(edge_index);
            }
        }
        center_node = "center_0";
        
        // 设置节点间延迟
        setupNodeLatencies(topu_radius);
    }

    void TearDown() override {
        // 确保所有日志都被刷新到文件
        spdlog::shutdown();
    }
    
    // 修改延迟参数范围
    // 这里的延时全部是单程
    double min_hop_latency = 1.0;   // 最小跳延迟 2ms
    double max_hop_latency = 2.0;   // 最大跳延迟 4ms
    double min_center_latency = 7.0; // 最小中心延迟 10ms
    double max_center_latency = 10.0; // 最大中心延迟 20ms
    // 添加用户到边缘服务器的延迟范围配置
    double min_user_latency = 1.0;  // 最小延迟 1ms
    double max_user_latency = 2.0;  // 最大延迟 2ms
    
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
    
    void setupNodeLatencies(int topu_radius) {
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
    int topu_radius = 8;
    int hops_lat = 4;
    int index_constran_lat = 3;
    int center_lat = 10;
    uint32_t block_size_ = 512; // KB
    uint32_t max_store_block_num = 150;
    uint32_t max_stream_num = 3584;
    uint32_t centarl_edge_node_access_frequency = 100000;     // 中心节点的基准访问频次
    double edge_node_decay_factor = 0.4;             // 边缘节点的访问频次衰减因子（相对于中心节点）
    double network_bandwidth = 100; // 100Mbps
    double ssd_read_block_ms = 2; // SSD读取一个block的耗时
    double center_compute_ms = 2; // center计算一个数据块的大约时间
    double edge_compute_ms = 4; // edge计算一个数据块的大约时间
    double network_trans_ms = 42; // 一个数据块的网络传输时间
    int time_range[4] = {1, 3, 5, 7};
    double selectivity_range[6] = {0.01, 0.1, 0.1, 0.2, 0.9, 0.9};

    bool split_stream = true;
    uint32_t max_cache_block_num = 2048;
    CacheStrategy cache_strategy_type = CacheStrategy::LECS;
    ZipfDistribution prepare_block_zipf{max_store_block_num, 0.6};  // 块访问的 Zipf 分布
    ZipfDistribution test_block_zipf{max_store_block_num, 0.6};

    // 添加生成用户延迟的方法
    double generateUserLatency() {
        std::uniform_real_distribution<double> dist(min_user_latency, max_user_latency);
        return dist(rng);
    }

    uint32_t sampleStream(const std::pair<uint32_t, uint32_t>& preferred_range, uint32_t max_stream_num) {
        if (!split_stream) {
            // split_stream=false的时候，所有边缘服务器都访问全局数据流
            std::uniform_int_distribution<uint32_t> global_dist(0, max_stream_num - 1);
            return global_dist(rng);
        }
        // 90% 概率选择本地流，10% 概率选择远程流
        std::bernoulli_distribution local_choice(0.90);
        
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
        const std::unordered_map<std::string, std::unordered_map<std::string, double>>& selectivity_records,
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
                        double selectivity = selectivity_records.at(access_node).at(block_key);
                        if (cache_strategy_type == CacheStrategy::LECS) {
                          // spdlog::info("i'am lecs");
                          double center_lat = access_count * edge_indices[access_node]->getNodeLatency(center_node) * 2;
                          double cache_lat = access_count * edge_indices[access_node]->getNodeLatency(cache_node) * 2;
                          qccv += static_cast<double>(center_lat - cache_lat);// + (1 - selectivity);
                        } else if (cache_strategy_type == CacheStrategy::TRECS) {
                          double center_lat = access_count * (ssd_read_block_ms + edge_indices[access_node]->getNodeLatency(center_node) * 2 + network_trans_ms * selectivity);
                          double cache_lat = access_count * (edge_indices[access_node]->getNodeLatency(cache_node) * 2 + network_trans_ms * selectivity);
                          if (access_node == cache_node) {
                            cache_lat -= access_count * network_trans_ms * selectivity;
                          }
                          qccv += static_cast<double>(center_lat - cache_lat);
                        } else {
                          spdlog::error("unknown cache strategy type {} when calculating QCCV", cache_strategy_type);
                        }
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
    }
    // 在类成员变量中添加随机数生成器
    std::mt19937 rng{std::random_device{}()};

    // 新增函数：生成访问记录和分布
    void generateAccessRecords(
        const std::unordered_map<std::string, uint32_t>& node_base_frequencies,
        const std::unordered_map<std::string, std::pair<uint32_t, uint32_t>>& node_stream_ranges,
        std::unordered_map<std::string, std::unordered_map<std::string, uint32_t>>& access_records,
        std::unordered_map<std::string, std::unordered_map<std::string, double>>& selectivity_records,
        std::set<std::string>& access_blocks_tran_set) {
        
        int high_selectivity_request = 0;
        int low_selectivity_request = 0;

        std::random_device rd; // 随机数生成器
        std::mt19937 gen(rd()); // 随机数引擎
        // 第一轮：生成访问记录和分布
        for (const auto& [node, base_freq] : node_base_frequencies) {
            // 生成访问记录
            for (uint32_t query = 0; query < base_freq; query++) {
                int query_blocks = time_range[query % 4];
                uint32_t selected_stream = sampleStream(node_stream_ranges.at(node), max_stream_num);
                uint32_t selected_block = max_store_block_num - prepare_block_zipf.sample();
                while (selected_block < query_blocks) {
                    selected_block = max_store_block_num - prepare_block_zipf.sample();
                }

                std::uniform_real_distribution<double> selectivity_dist(selectivity_range[selected_stream % 6], selectivity_range[selected_stream % 6]); // 选择性分布
                // 访问连续的query_blocks个块
                for (int i = 0; i < query_blocks; i++) {
                    std::string block_key = std::to_string(selected_stream) + ":" + 
                                          std::to_string(selected_block - i);
                    double selectivity = selectivity_dist(gen);
                    if (selectivity > 0.5) {
                        high_selectivity_request++;
                    } else {
                        low_selectivity_request++;
                    }
                    access_records[node][block_key] += 1;
                    selectivity_records[node][block_key] += selectivity;
                    access_blocks_tran_set.insert(block_key);
                }
            }
        }
        spdlog::info("access_blocks_tran_set: {}", access_blocks_tran_set.size());
        spdlog::info("trans: high_selectivity_request {}, low_selectivity_request {}", high_selectivity_request, low_selectivity_request);
    }

    // 新增函数：模拟一轮缓存分配
    std::unordered_map<std::string, std::unordered_set<std::string>> simulateCacheAllocationRound(
        const std::vector<std::string>& edge_nodes,
        uint32_t topu_radius,
        std::unordered_map<std::string, std::pair<uint32_t, uint32_t>> &node_stream_ranges,
        std::unordered_map<std::string, uint32_t> &node_base_frequencies,
        uint32_t centarl_edge_node_access_frequency,
        double edge_node_decay_factor,
        std::unordered_map<std::string, std::unordered_map<std::string, uint32_t>>& access_records,
        std::unordered_map<std::string, std::unordered_map<std::string, double>>& selectivity_records,
        std::set<std::string>& access_blocks_tran_set) {
        
        uint32_t range_size = max_stream_num / (topu_radius * topu_radius);  // 每个节点偏好的流范围大小
        
        // 计算每个节点的基础访问频率（基于到中心的曼哈顿距离）
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
            if (split_stream) {
                node_stream_ranges[node] = {start_stream, end_stream};
            } else {
                node_stream_ranges[node] = {0, max_stream_num};
            }
            spdlog::info("node: {}, start_stream: {}, end_stream: {}, freq: {}", node, start_stream, end_stream, base_freq);
        }
        // 调用新函数生成访问记录
        generateAccessRecords(node_base_frequencies, node_stream_ranges, access_records, selectivity_records, access_blocks_tran_set);
        
        for (auto &[node, mp] : access_records) {
            for (auto &[block_key, count] : mp) {
                // spdlog::info("before {}, {}", count, selectivity_records[node][block_key]);
                selectivity_records[node][block_key] /= (count + 0.0);
                // spdlog::info("atfter {}, {}", count, selectivity_records[node][block_key]);
            }
        }
        
        // 使用类成员方法生成和执行分配方案
        auto allocation_plan = generateAllocationPlan(access_blocks_tran_set, access_records, selectivity_records, node_stream_ranges, node_base_frequencies);
        executeAllocationPlan(allocation_plan);
        return allocation_plan;
    }

    // 在类成员变量中添加存储延时的容器
    std::vector<double> query_latencies; // 存储每个查询的延时
};

// 更新基本拓扑测试以适应新的延迟范围
TEST_F(EdgeCacheTRECSTest, QueryTest) {
    // 为每个节点分配其偏好的数据流范围
    std::unordered_map<std::string, std::pair<uint32_t, uint32_t>> node_stream_ranges;
    uint32_t range_size = max_stream_num / (topu_radius * topu_radius);  // 每个节点偏好的流范围大小
    
    // 计算每个节点的基础访问频率（基于到中心的曼哈顿距离）
    std::unordered_map<std::string, uint32_t> node_base_frequencies;
    int center_i = topu_radius / 2;
    int center_j = topu_radius / 2;
    int max_manhattan_dist = topu_radius - 1;  // 最大曼哈顿距离就是从中心到边缘的距离
        std::unordered_map<std::string, std::unordered_map<std::string, uint32_t>> 
        access_records; // node -> block_key -> access_count
    std::unordered_map<std::string, std::unordered_map<std::string, double>> selectivity_records;
    std::set<std::string> access_blocks_tran_set;
    spdlog::info("central_edge_node i: {}, j: {}", center_i, center_j);
    // 调用模拟缓存分配的函数
    auto allocation_plan = simulateCacheAllocationRound(edge_nodes, topu_radius, node_stream_ranges, node_base_frequencies,
        centarl_edge_node_access_frequency, edge_node_decay_factor, access_records, selectivity_records, access_blocks_tran_set);

    int high_selectivity_allocate_block = 0;
    int low_selectivity_allocate_block = 0;
    for (auto &[node, mp] : allocation_plan) {
        for (auto &block_key : mp) {
            if (selectivity_records[node][block_key] > 0.5) {
                high_selectivity_allocate_block++;
            } else {
                low_selectivity_allocate_block++;
            }
        }
    }

    // 第二轮：验证索引
    uint64_t total_queries = 0;
    uint64_t cache_hit_count = 0;
    uint64_t request_count = 0;
    std::set<std::string> access_blocks_test_set;

    // 添加两组延时统计相关变量
    double total_query_latencies = 0;    // 实际发生的延时
    double total_sub_query_latency = 0; // 新增变量以存储所有子查询延时的总和
    double total_trans_latencies = 0; // 所有延时中，网络传输延时的总量
    double total_prop_latencies = 0;
    double network_flow_KB = 0;  // 集群内的总网络流量
    double high_selectivity_count = 0;
    double high_selectivity_local_hit = 0;
    std::set<std::string> high_selectivity_block_num;
    std::uniform_real_distribution<> jitter_dist(1.0, 2.0);
    
    std::random_device rd; // 用于选择率的随机数生成器
    std::mt19937 gen(rd()); // 用于选择率的随机数引擎
    std::set<string> all_access_block_set;
    for (const auto& [node, base_freq] : node_base_frequencies) {
        spdlog::info("query phase: node: {}, base_freq: {}", node, base_freq);
        // 模拟查询
        for (uint32_t query = 0; query < base_freq / 8; query++) {
            int query_blocks = time_range[query % 4];
            uint32_t selected_stream = sampleStream(node_stream_ranges[node], max_stream_num);
            uint32_t selected_block = max_store_block_num - test_block_zipf.sample();
            while (selected_block < query_blocks) {
                selected_block = max_store_block_num - test_block_zipf.sample();
            }

            // 为整个查询生成一个用户延迟（假设用户位置在查询期间不变）
            double user_latency = generateUserLatency();
            double total_data = 0;
            double max_sub_query_latency = 0;
            total_queries++;
            
            std::uniform_real_distribution<double> selectivity_dist(selectivity_range[selected_stream % 6], selectivity_range[selected_stream % 6]); // 选择性分布
            // 查询连续的query_blocks个块
            std::map<std::string, int> all_sub_node;
            std::vector<double> all_sub_node_lat(query_blocks, 0.0);
            for (int i = 0; i < query_blocks; i++) {
                request_count++;
                uint32_t current_block = selected_block - i;
                std::string block_key = std::to_string(selected_stream) + ":" + 
                                      std::to_string(current_block);
                all_access_block_set.insert(block_key);

                double selectivity = selectivity_dist(gen);
                double sub_query_latency = 0;

                total_data += selectivity;

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
                } else {
                    found_node = center_node;
                }
                if (edge_indices[node]->getNodeLatency(center_node) <= edge_indices[node]->getNodeLatency(found_node)) {
                    // std::cout<<"tesyes" << std::endl;
                    found_node = center_node;
                }
                const auto& node_blocks = allocation_plan[found_node];
                if (node_blocks.find(block_key) == node_blocks.end()) {
                    // 假阳性
                    found_node = center_node;
                }
                // 到这里found_node就指向了数据真正存在的节点
                if (found_node == node) {
                    cache_hit_count++;
                    if (selectivity > 0.5) {
                        high_selectivity_local_hit++;
                    }
                    // 本地查询
                    if (all_sub_node.find(found_node) != all_sub_node.end()) {
                        sub_query_latency = all_sub_node_lat[all_sub_node[found_node]] + edge_compute_ms;
                    } else {
                        sub_query_latency = edge_compute_ms;
                    }
                } else if (found_node == center_node) {
                    network_flow_KB += block_size_ * selectivity;
                    // 访问中心云
                    if (all_sub_node.find(found_node) != all_sub_node.end()) {
                        sub_query_latency = all_sub_node_lat[all_sub_node[found_node]] + ssd_read_block_ms + center_compute_ms + network_trans_ms * selectivity;
                    } else {
                        sub_query_latency = ssd_read_block_ms + center_compute_ms + network_trans_ms * selectivity + edge_indices[node]->getNodeLatency(center_node) * 2;
                    }
                    total_trans_latencies += network_trans_ms * selectivity;
                    total_prop_latencies += edge_indices[node]->getNodeLatency(center_node) * 2;
                } else {
                    cache_hit_count++;
                    // 非本地查询，多计算一跳网络流量
                    network_flow_KB += block_size_ * selectivity;
                    if (all_sub_node.find(found_node) != all_sub_node.end()) {
                        sub_query_latency = all_sub_node_lat[all_sub_node[found_node]] + edge_compute_ms + network_trans_ms * selectivity;
                    } else {
                        sub_query_latency = edge_compute_ms + network_trans_ms * selectivity + edge_indices[node]->getNodeLatency(found_node) * 2;
                    }
                    total_trans_latencies += network_trans_ms * selectivity;
                    // spdlog::info("network_flow: {}KB.  network_trans_lat: {}ms", block_size_ * selectivity, network_trans_ms * selectivity);
                    total_prop_latencies += edge_indices[node]->getNodeLatency(found_node) * 2;
                }
                if (selectivity > 0.5) {
                    high_selectivity_count++;
                    high_selectivity_block_num.insert(block_key);
                    // spdlog::info("high selectivity request: {}ms", sub_query_latency);
                }
                all_sub_node[found_node] = i;
                all_sub_node_lat[i] = sub_query_latency;
                max_sub_query_latency = std::max(max_sub_query_latency, sub_query_latency);
            }
            total_sub_query_latency += max_sub_query_latency; // 累加子查询延时
            // network_flow_KB += total_data * block_size_;
            // double single_query_latency = edge_compute_ms + max_sub_query_latency + total_data * network_trans_ms + user_latency * 2;
            double single_query_latency = edge_compute_ms / 2 + max_sub_query_latency + user_latency * 2;
            total_query_latencies += single_query_latency;

            // 在查询循环中，记录每个子查询的延时
            query_latencies.push_back(single_query_latency);
        }
    }
    
    // 在查询结束后，计算平均延时和p90延时
    double average_latency = total_query_latencies / total_queries;
    std::sort(query_latencies.begin(), query_latencies.end());
    double p10_latency = query_latencies[static_cast<size_t>(query_latencies.size() * 0.1)];
    double p30_latency = query_latencies[static_cast<size_t>(query_latencies.size() * 0.3)];
    double p50_latency = query_latencies[static_cast<size_t>(query_latencies.size() * 0.5)];
    double p70_latency = query_latencies[static_cast<size_t>(query_latencies.size() * 0.7)];
    double p90_latency = query_latencies[static_cast<size_t>(query_latencies.size() * 0.9)];

    // 在查询结束后，计算平均子查询延时
    double average_sub_query_latency = total_sub_query_latency / (total_queries + 0.1); // 避免除以零
    spdlog::info("Average Sub Query Latency: {}ms", average_sub_query_latency); // 输出平均子查询延时

    // 输出统计信息
    spdlog::info("Total Queries: {}", total_queries);
    spdlog::info("Cache Hit Count: {}", cache_hit_count);
    spdlog::info("Cache Request Count: {}", request_count);
    spdlog::info("Cache Hit Rate: {}", (cache_hit_count / (request_count + 0.1)) * 100);
    spdlog::info("High selectivity block Count: {}", high_selectivity_block_num.size());
    spdlog::info("High selectivity local Cache Hit Count: {}", high_selectivity_local_hit);
    spdlog::info("High selectivity total Count: {}", high_selectivity_count);
    spdlog::info("Total Network Flow (KB): {}", network_flow_KB);
    spdlog::info("Total Network Trans Latency: {}ms", total_trans_latencies);
    spdlog::info("Total Network Propgation Latency: {}ms", total_prop_latencies);
    spdlog::info("Total Latency: {}ms", total_query_latencies);
    spdlog::info("Average Latency: {}ms", average_latency);
    spdlog::info("P10 Latency: {}ms", p10_latency);
    spdlog::info("P30 Latency: {}ms", p30_latency);
    spdlog::info("P50 Latency: {}ms", p50_latency);
    spdlog::info("P70 Latency: {}ms", p70_latency);
    spdlog::info("P90 Latency: {}ms", p90_latency);

    // 在文件最后输出这两个变量的值
    spdlog::info("High Selectivity Allocate Block: {}", high_selectivity_allocate_block);
    spdlog::info("Low Selectivity Allocate Block: {}", low_selectivity_allocate_block);
    spdlog::info("All Cached Block: {}", topu_radius * topu_radius * max_cache_block_num); 
    spdlog::info("All Access BlockSet: {}", all_access_block_set.size());
}