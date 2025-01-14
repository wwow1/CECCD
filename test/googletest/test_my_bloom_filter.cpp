#include <gtest/gtest.h>
#include "edge_cache_index.h"
#include "config_manager.h"
#include <string>
#include <vector>
#include <random>
#include <thread>
#include <unordered_set>

class MyBloomFilterTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 设置测试用的配置参数
        auto& config = ConfigManager::getInstance();
        
        config.loadConfig("config/cluster_config.json");        
        // 创建布隆过滤器
        filter = std::make_unique<MyBloomFilter>();
    }

    std::unique_ptr<MyBloomFilter> filter;
};

// 测试基本的添加和查询功能
TEST_F(MyBloomFilterTest, BasicAddAndQuery) {
    uint32_t datastreamID = 1;
    uint32_t blockId1 = 100;
    uint32_t blockId2 = 200;
    
    // 添加blockId1并验证
    std::string key = std::to_string(datastreamID) + ":" + std::to_string(blockId1);
    std::cout << "Adding key: " << key << std::endl;
    
    filter->add(datastreamID, blockId1);
    std::vector<uint32_t> result = filter->range_query(datastreamID, blockId1, blockId1);
    
    std::cout << "Query result size: " << result.size() << std::endl;
    if (!result.empty()) {
        std::cout << "First result: " << result[0] << std::endl;
    }
    
    EXPECT_EQ(result.size(), 1) << "Expected one result for key: " << key;
    if (!result.empty()) {
        EXPECT_EQ(result[0], blockId1) << "Expected result to match input blockId";
    }
    
    // 验证未添加的blockId2不会匹配
    result = filter->range_query(datastreamID, blockId2, blockId2);
    EXPECT_TRUE(result.empty()) << "Expected no results for unadded blockId";
}

// 测试删除功能
TEST_F(MyBloomFilterTest, RemoveTest) {
    uint32_t datastreamID = 1;
    uint32_t blockId = 100;
    
    // 添加并验证
    filter->add(datastreamID, blockId);
    std::vector<uint32_t> result = filter->range_query(datastreamID, blockId, blockId);
    EXPECT_FALSE(result.empty());
    
    // 删除并验证
    filter->remove(datastreamID, blockId);
    result = filter->range_query(datastreamID, blockId, blockId);
    EXPECT_TRUE(result.empty());
}

// 测试范围查询功能
TEST_F(MyBloomFilterTest, RangeQueryTest) {
    uint32_t datastreamID = 1;
    std::vector<uint32_t> inserted_blocks = {100, 101, 102, 103, 104};
    
    // 插入连续的块
    for (uint32_t blockId : inserted_blocks) {
        filter->add(datastreamID, blockId);
    }
    
    // 测试完整范围查询
    std::vector<uint32_t> result = filter->range_query(datastreamID, 100, 104);
    EXPECT_EQ(result.size(), inserted_blocks.size());
    
    // 测试部分范围查询
    result = filter->range_query(datastreamID, 101, 103);
    EXPECT_EQ(result.size(), 3);
}

// 测试多线程并发访问
TEST_F(MyBloomFilterTest, ConcurrentAccess) {
    const uint32_t datastreamID = 1;
    const int num_threads = 4;
    const int ops_per_thread = 1000;
    
    std::vector<std::thread> threads;
    
    // 创建多个线程同时进行读写操作
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back([this, datastreamID, i, ops_per_thread]() {
            for (int j = 0; j < ops_per_thread; j++) {
                uint32_t blockId = i * ops_per_thread + j;
                filter->add(datastreamID, blockId);
                filter->range_query(datastreamID, blockId, blockId);
                if (j % 2 == 0) {
                    filter->remove(datastreamID, blockId);
                }
            }
        });
    }
    
    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }
    
    // 验证最终状态
    std::vector<uint32_t> result = filter->range_query(datastreamID, 0, num_threads * ops_per_thread);
    EXPECT_FALSE(result.empty());
}

// 测试不同数据流ID
TEST_F(MyBloomFilterTest, MultipleDatastreams) {
    uint32_t blockId = 100;
    std::vector<uint32_t> datastreamIDs = {1, 2, 3};
    
    // 在不同的数据流中添加相同的块ID
    for (uint32_t datastreamID : datastreamIDs) {
        filter->add(datastreamID, blockId);
    }
    
    // 验证每个数据流都能正确查询到
    for (uint32_t datastreamID : datastreamIDs) {
        std::vector<uint32_t> result = filter->range_query(datastreamID, blockId, blockId);
        EXPECT_EQ(result.size(), 1);
        EXPECT_EQ(result[0], blockId);
    }
}

// 测试实际场景下的假阳性率
TEST_F(MyBloomFilterTest, RealWorldFalsePositiveRate) {
    const uint32_t datastreamID = 1;
    const int block_size_mb = 1;  // 1MB
    const double edge_capacity_gb = 2.0;  // 2GB
    const int num_blocks = static_cast<int>((edge_capacity_gb * 1024) / block_size_mb);  // 2048 blocks
    const int num_queries = 10000;  // 进行1万次查询
    const int blocks_per_query = 5;  // 每次查询5个连续的块
    
    // 使用随机数生成器
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> insert_dist(1, 10000);
    std::uniform_int_distribution<uint32_t> query_ratio_dist(1, 100);  // 用于决定每个块是否存在
    
    // 记录插入的数字
    std::unordered_set<uint32_t> inserted_set;
    
    // 插入随机数据
    std::cout << "Inserting " << num_blocks << " blocks..." << std::endl;
    for (int i = 0; i < num_blocks; i++) {
        uint32_t blockId = insert_dist(gen);
        while (inserted_set.count(blockId) > 0) {  // 确保不重复
            blockId = insert_dist(gen);
        }
        filter->add(datastreamID, blockId);
        inserted_set.insert(blockId);
    }
    
    // 统计查询结果
    int false_positive_queries = 0;  // 包含假阳性的查询次数
    int total_queries_with_nonexistent = 0;  // 包含不存在块的查询次数
    
    std::cout << "Performing " << num_queries << " range queries..." << std::endl;
    for (int i = 0; i < num_queries; i++) {
        std::vector<uint32_t> query_blocks;
        bool has_nonexistent = false;
        std::unordered_set<uint32_t> nonexistent_blocks;
        
        // 生成这次查询要用的所有块ID
        uint32_t base_block = insert_dist(gen);
        for (int j = 0; j < blocks_per_query; j++) {
            uint32_t blockId = base_block + j;
            int ratio = query_ratio_dist(gen);
            
            if (ratio <= 80) {  // 80%概率使用已存在的块
                while (inserted_set.count(blockId) == 0) {
                    blockId = insert_dist(gen);
                }
            } else {  // 20%概率使用不存在的块
                while (inserted_set.count(blockId) > 0) {
                    blockId = insert_dist(gen);
                }
                has_nonexistent = true;
                nonexistent_blocks.insert(blockId);
            }
            query_blocks.push_back(blockId);
        }
        
        // 只有查询包含不存在的块时才计入统计
        if (has_nonexistent) {
            total_queries_with_nonexistent++;
            
            // 对每个不存在的块进行单独查询
            bool has_false_positive = false;
            for (uint32_t blockId : query_blocks) {
                if (nonexistent_blocks.count(blockId) > 0) {  // 只查询不存在的块
                    std::vector<uint32_t> result = filter->range_query(datastreamID, blockId, blockId);
                    if (!result.empty()) {
                        has_false_positive = true;
                        break;  // 一个查询中发现一个假阳性就足够了
                    }
                }
            }
            
            if (has_false_positive) {
                false_positive_queries++;
            }
        }
    }
    
    // 计算查询级别的假阳性率
    double query_fpr = static_cast<double>(false_positive_queries) / total_queries_with_nonexistent;
    std::cout << "Total queries with nonexistent blocks: " << total_queries_with_nonexistent << std::endl;
    std::cout << "Queries with false positives: " << false_positive_queries << std::endl;
    std::cout << "Query-level False Positive Rate: " << query_fpr << std::endl;
}