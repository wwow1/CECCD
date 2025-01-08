#include <gtest/gtest.h>
#include "edge_cache_index.h"
#include "config_manager.h"
#include <string>
#include <vector>
#include <random>
#include <thread>

class MyBloomFilterTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 设置测试用的配置参数
        auto& config = ConfigManager::getInstance();
        
        // 创建一个临时的 JSON 配置
        nlohmann::json test_config = {
            {"cluster", {
                {"center_node", {{"address", "127.0.0.1"}}},
                {"edge_capacity_gb", 1.0},      // 1GB
                {"block_size_mb", 1},           // 1MB
                {"bloom_filter_fpr", 0.01},     // 1% 误判率
                {"index_latency_threshold_ms", 30},
                {"statistics_report_interval_s", 60},
                {"prediction_period_s", 300}
            }},
            {"database", {
                {"host", "localhost"},
                {"port", "5432"},
                {"dbname", "testdb"},
                {"user", "testuser"},
                {"password", "testpass"}
            }}
        };
        
        // 将配置写入临时文件
        std::string test_config_path = "test_config.json";
        std::ofstream config_file(test_config_path);
        config_file << test_config.dump(4);
        config_file.close();
        
        // 加载测试配置
        config.loadConfig(test_config_path);
        
        // 删除临时配置文件
        std::remove(test_config_path.c_str());
        
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