#include <iostream>
#include <string>
#include <vector>
#include "../include/countingBloomFilter.hpp" // 请替换为您的文件路径

using namespace elastic_rose;

int main() {
    // Bloom Filter的参数
    int m = 256;               // Bloom Filter的位数
    double false_positive = 0.01; // 期望的误报率
    uint32_t id = 1;            // 哈希函数的ID

    // 创建CountingBloomFilter实例
    CountingBloomFilter filter(m, false_positive, id);

    // 获取期望的元素数目
    size_t expect_num = filter.GetExpectNum();
    std::cout << "Expecting to insert about " << expect_num << " keys." << std::endl;

    // 插入大约2倍于expect_num数量的键
    size_t test_insert_count = expect_num * 2;
    std::cout << "Attempting to insert " << test_insert_count << " keys to test capacity handling." << std::endl;

    bool insertion_failed = false;
    for (size_t i = 0; i < test_insert_count; ++i) {
        std::string key = "key" + std::to_string(i);
        if (!filter.PutKey(key)) {
            std::cout << "Insertion failed for key: " << key << " at index " << i << std::endl;
            insertion_failed = true;
            break;
        }
        if (!filter.KeyMayMatch(key)) {
            std::cout << "Key not found after insertion: " << key << std::endl;
            insertion_failed = true;
            break;
        }
    }

    if (!insertion_failed) {
        std::cout << "Successfully inserted " << test_insert_count << " keys without reaching capacity." << std::endl;
    } else {
        std::cout << "Insertion stopped due to capacity or overflow." << std::endl;
    }

    // 测试删除部分键
    std::cout << "Testing deletion of some keys." << std::endl;
    for (size_t i = 0; i < test_insert_count / 2; ++i) {
        std::string key = "key" + std::to_string(i);
        if (!filter.DeleteKey(key)) {
            std::cout << "Deletion failed for key: " << key << std::endl;
            return -1;
        }
        if (filter.KeyMayMatch(key)) {
            std::cout << "Key still found after deletion: " << key << std::endl;
        }
    }
    std::cout << "Deletion and verification passed for half of the inserted keys." << std::endl;

    // 检查计数器溢出
    std::string overflowKey = "test_overflow";
    bool overflowOccurred = false;
    for (int i = 0; i <= filter.GetMaxCounterValue(); ++i) {
        if (!filter.PutKey(overflowKey)) {
            std::cout << "Counter overflow triggered correctly at insert count: " << i << std::endl;
            overflowOccurred = true;
            break;
        }
    }
    if (!overflowOccurred) {
        std::cout << "Warning: Counter overflow test did not trigger overflow." << std::endl;
    }

    std::cout << "All tests completed." << std::endl;
    return 0;
}
