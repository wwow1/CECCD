#include <gtest/gtest.h>
#include "countingBloomFilter.hpp"
#include <string>
#include <vector>
#include <random>

class CountingBloomFilterTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 设置1MB大小的过滤器，假阳性率为0.01
        filter = elastic_rose::CountingBloomFilter(1024 * 1024, 0.01, 0);
    }

    elastic_rose::CountingBloomFilter filter;
};

// 测试基本的添加和查询功能
TEST_F(CountingBloomFilterTest, BasicAddAndQuery) {
    std::string key1 = "test_key_1";
    std::string key2 = "test_key_2";
    
    // 添加key1并验证
    EXPECT_TRUE(filter.Add(key1));
    EXPECT_TRUE(filter.KeyMayMatch(key1));
    
    // 验证未添加的key2不会匹配
    EXPECT_FALSE(filter.KeyMayMatch(key2));
}

// 测试删除功能
TEST_F(CountingBloomFilterTest, RemoveTest) {
    std::string key = "test_key";
    
    // 添加并验证
    EXPECT_TRUE(filter.Add(key));
    EXPECT_TRUE(filter.KeyMayMatch(key));
    
    // 删除并验证
    EXPECT_TRUE(filter.Remove(key));
    EXPECT_FALSE(filter.KeyMayMatch(key));
}

// 测试多次添加删除
TEST_F(CountingBloomFilterTest, MultipleAddRemove) {
    std::string key = "test_key";
    
    // 添加两次
    EXPECT_TRUE(filter.Add(key));
    EXPECT_TRUE(filter.Add(key));
    EXPECT_TRUE(filter.KeyMayMatch(key));
    
    // 删除一次，应该仍然存在
    EXPECT_TRUE(filter.Remove(key));
    EXPECT_TRUE(filter.KeyMayMatch(key));
    
    // 再次删除，应该不存在
    EXPECT_TRUE(filter.Remove(key));
    EXPECT_FALSE(filter.KeyMayMatch(key));
}

// 测试大量数据的插入
TEST_F(CountingBloomFilterTest, BulkInsert) {
    std::vector<std::string> keys;
    const int num_keys = 10000;
    
    // 生成随机键
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 1000000);
    
    for (int i = 0; i < num_keys; i++) {
        keys.push_back("key_" + std::to_string(dis(gen)));
    }
    
    // 插入所有键
    for (const auto& key : keys) {
        EXPECT_TRUE(filter.Add(key));
    }
    
    // 验证所有插入的键都能被找到
    for (const auto& key : keys) {
        EXPECT_TRUE(filter.KeyMayMatch(key));
    }
    
    // 验证过滤器的统计信息
    EXPECT_EQ(filter.GetInsertNum(), num_keys);
    EXPECT_GT(filter.GetExpectNum(), 0);
}

// 测试计数器溢出情况
TEST_F(CountingBloomFilterTest, CounterOverflow) {
    std::string key = "test_key";
    
    // 重复添加直到某个计数器溢出
    bool overflow_detected = false;
    for (int i = 0; i < 300; i++) {
        if (!filter.Add(key)) {
            overflow_detected = true;
            break;
        }
    }
    
    EXPECT_TRUE(overflow_detected);
}

// 测试预期元素数量超出
TEST_F(CountingBloomFilterTest, ExpectedNumExceeded) {
    // 创建一个较小的过滤器
    elastic_rose::CountingBloomFilter small_filter(1024, 0.01, 0);
    std::string base_key = "test_key_";
    
    // 添加大量元素直到超出预期数量
    bool limit_detected = false;
    for (int i = 0; i < 10000; i++) {
        if (!small_filter.Add(base_key + std::to_string(i))) {
            limit_detected = true;
            break;
        }
    }
    
    EXPECT_TRUE(limit_detected);
} 