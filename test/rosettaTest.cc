#include <iostream>
#include <random>
#include <chrono>
#include <unordered_set>
#include "../include/rosetta.hpp"

using namespace elastic_rose;

void testRosetta(u32 total_size, u32 alpha, double beta, double false_positive, u64 test_count, u64 range) {
    // 初始化 Rosetta
    Rosetta rosetta(total_size, alpha, beta, false_positive);
    
    // 随机数生成器
    std::mt19937_64 rng(std::random_device{}());
    std::uniform_int_distribution<u64> dist(0, range);

    // 数据集和插入阶段
    std::unordered_set<u64> dataset;
    std::vector<u64> inserted_keys;
    auto now = std::chrono::system_clock::now();
    for (u64 i = 0; i < test_count / 2; ++i) {
        u64 key = dist(rng);
        if (dataset.insert(key).second) { // 避免重复插入
            rosetta.insertKey(key);
            inserted_keys.push_back(key);
        }
    }
    auto end = std::chrono::system_clock::now();
    // 测试删除阶段
    std::unordered_set<u64> deleted_keys;
    for (u64 i = 0; i < inserted_keys.size() / 2; ++i) {
        u64 key = inserted_keys[i];
        rosetta.DeleteKey(key);
        dataset.erase(key);
        deleted_keys.insert(key);
    }

    // 查询测试阶段
    u64 false_positives = 0;
    u64 true_positives = 0;
    u64 false_negatives = 0;

    for (u64 i = 0; i < inserted_keys.size(); ++i) {
        bool in_rosetta = rosetta.range_query(inserted_keys[i], inserted_keys[i]);
    }
    // 输出统计结果
    std::cout << "Test Results:\n";
    std::cout << "Total Queries: " << inserted_keys.size() << "\n";
    std::cout << "True Positives: " << true_positives << "\n";
    std::cout << "False Positives: " << false_positives << "\n";
    std::cout << "False Negatives: " << false_negatives << "\n";
    std::cout << "False Positive Rate: " << static_cast<double>(false_positives) / test_count << "\n";
    auto duration = end - now;
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    std::cout << "exec time: " << duration_ms  << "\n";
}

int main() {
    // 配置参数
    u32 total_size = 1 << 25;  // 总大小
    u32 alpha = 8;             // 位差
    double beta = 0.9;         // 空间差异
    double false_positive = 0.01; // 目标假阳性率
    u64 test_count = 1e6;      // 测试次数
    u64 range = 1e9;           // 随机数范围

    // 运行测试
    testRosetta(total_size, alpha, beta, false_positive, test_count, range);
    return 0;
}
