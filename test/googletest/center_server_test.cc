#include <gtest/gtest.h>
#include <chrono>
#include <random>
#define private public
#include "center_server.h"
// 测试类
class CenterServerTest : public ::testing::Test {
protected:
    CenterServer server;

    void SetUp() override {
        // 初始化设置
    }

    void TearDown() override {
        // 清理工作
    }
};

// 保留原有的测试逻辑
TEST_F(CenterServerTest, TestCacheReplacementLoopPerformance) {
    const int num_nodes = 20;
    const int blocks_per_node = 2048; // 2GB / 1MB
    const int total_blocks = num_nodes * blocks_per_node;

    // 随机数生成器用于访问频率和选择率
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> access_dist(0, 100);
    std::uniform_real_distribution<double> selectivity_dist(0.0, 1.0);

    // 模拟数据：为每个块生成随机访问频率和选择率
    for (int node = 0; node < num_nodes; ++node) {
        std::string node_address = "node" + std::to_string(node);
        for (int block = 0; block < total_blocks; ++block) {
            std::string block_key = std::to_string(block);
            CenterServer::NodeStats stats = {access_dist(rng), selectivity_dist(rng)};
            server.current_period_stats_[block_key][node_address] = stats;
        }
    }
    std::cout << "block number: " << server.current_period_stats_.size() << std::endl;
    std::cout << "node number: " << server.current_period_stats_.begin()->second.size() << std::endl;
    auto start = std::chrono::high_resolution_clock::now();
    auto period_stats = server.collectPeriodStats();
    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "collectPeriodStats execution time: " 
              << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() 
              << " ms" << std::endl;

    start = std::chrono::high_resolution_clock::now();
    server.updateAccessHistory(period_stats);
    end = std::chrono::high_resolution_clock::now();
    std::cout << "updateAccessHistory execution time: " 
              << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() 
              << " ms" << std::endl;

    start = std::chrono::high_resolution_clock::now();
    server.calculateQCCVs();
    end = std::chrono::high_resolution_clock::now();
    std::cout << "calculateQCCVs execution time: " 
              << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() 
              << " ms" << std::endl;

    std::cout << "Cache replacement loop iteration completed" << std::endl;
    // 你可以在这里添加断言
}