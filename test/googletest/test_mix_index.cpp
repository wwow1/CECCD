#include <gtest/gtest.h>
#include "edge_cache_index.h"
#include <string>
#include <vector>
#include <thread>

class MixIndexTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建 MixIndex 实例
        index = std::make_unique<MixIndex>();
    }

    std::unique_ptr<MixIndex> index;
};

// 测试基本的添加和查询功能
TEST_F(MixIndexTest, BasicAddAndQuery) {
    uint32_t datastreamID = 1;
    uint32_t blockId1 = 100;
    uint32_t blockId2 = 200;
    
    // 添加blockId1并验证
    index->add(datastreamID, blockId1);
    std::vector<uint32_t> result = index->range_query(datastreamID, blockId1, blockId1);
    
    EXPECT_EQ(result.size(), 1) << "Expected one result for blockId: " << blockId1;
    if (!result.empty()) {
        EXPECT_EQ(result[0], blockId1) << "Expected result to match input blockId";
    }
    
    // 验证未添加的blockId2不会匹配
    result = index->range_query(datastreamID, blockId2, blockId2);
    EXPECT_TRUE(result.empty()) << "Expected no results for unadded blockId";
}

// 测试删除功能
TEST_F(MixIndexTest, RemoveTest) {
    uint32_t datastreamID = 1;
    uint32_t blockId = 100;
    
    // 添加并验证
    index->add(datastreamID, blockId);
    std::vector<uint32_t> result = index->range_query(datastreamID, blockId, blockId);
    EXPECT_FALSE(result.empty());
    
    // 删除并验证
    index->remove(datastreamID, blockId);
    result = index->range_query(datastreamID, blockId, blockId);
    EXPECT_TRUE(result.empty());
}

// 测试范围查询功能
TEST_F(MixIndexTest, RangeQueryTest) {
    uint32_t datastreamID = 1;
    std::vector<uint32_t> inserted_blocks = {100, 101, 102, 103, 104};
    
    // 插入连续的块
    for (uint32_t blockId : inserted_blocks) {
        index->add(datastreamID, blockId);
    }
    
    // 测试完整范围查询
    std::vector<uint32_t> result = index->range_query(datastreamID, 100, 104);
    EXPECT_EQ(result.size(), inserted_blocks.size());
    
    // 测试部分范围查询
    result = index->range_query(datastreamID, 101, 103);
    EXPECT_EQ(result.size(), 3);
}

// 测试多线程并发访问
TEST_F(MixIndexTest, ConcurrentAccess) {
    const uint32_t datastreamID = 1;
    const int num_threads = 4;
    const int ops_per_thread = 1000;
    
    std::vector<std::thread> threads;
    
    // 创建多个线程同时进行读写操作
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back([this, datastreamID, i, ops_per_thread]() {
            for (int j = 0; j < ops_per_thread; j++) {
                uint32_t blockId = i * ops_per_thread + j;
                index->add(datastreamID, blockId);
                index->range_query(datastreamID, blockId, blockId);
                if (j % 2 == 0) {
                    index->remove(datastreamID, blockId);
                }
            }
        });
    }
    
    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }
    
    // 验证最终状态
    std::vector<uint32_t> result = index->range_query(datastreamID, 0, num_threads * ops_per_thread);
    EXPECT_FALSE(result.empty());
}

// 测试不同数据流ID
TEST_F(MixIndexTest, MultipleDatastreams) {
    uint32_t blockId = 100;
    std::vector<uint32_t> datastreamIDs = {1, 2, 3};
    
    // 在不同的数据流中添加相同的块ID
    for (uint32_t datastreamID : datastreamIDs) {
        index->add(datastreamID, blockId);
    }
    
    // 验证每个数据流都能正确查询到
    for (uint32_t datastreamID : datastreamIDs) {
        std::vector<uint32_t> result = index->range_query(datastreamID, blockId, blockId);
        EXPECT_EQ(result.size(), 1);
        EXPECT_EQ(result[0], blockId);
    }
}