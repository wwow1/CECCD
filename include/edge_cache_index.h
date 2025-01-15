#ifndef EDGE_CACHE_INDEX_HPP
#define EDGE_CACHE_INDEX_HPP

#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <iostream>
#include "countingBloomFilter.hpp"
#include "common.h"
#include "cloud_edge_cache.grpc.pb.h"
#include "roaring.hh"
#include <tbb/concurrent_hash_map.h>
#include <shared_mutex>
#include "config_manager.h"
#include <spdlog/spdlog.h>

struct MyBloomFilter : public Common::BaseIndex {
    mutable std::shared_mutex mutex_;
    elastic_rose::CountingBloomFilter bloom_filter_;

    MyBloomFilter() {
        auto& config = ConfigManager::getInstance();
        
        // 计算edge节点可以存储的最大块数
        size_t max_blocks = (config.getEdgeCapacityGB() * 1024) / config.getBlockSizeMB();
        double fpr = config.getBloomFilterFPR();
        
        // 为了应对可能的临时超出和动态变化，我们将容量扩大1.2倍
        size_t num_items = static_cast<size_t>(max_blocks * 1.2);
        
        // 计算每个元素需要的比特数（bits per element）
        // 使用公式：bits_per_elem = -log(p) / (ln(2)^2)
        double bits_per_elem = -log(fpr) / (log(2) * log(2));
        
        // 计算总比特数
        size_t total_bytes = static_cast<size_t>(num_items * bits_per_elem);
        
        spdlog::info("Initializing BloomFilter: edge_capacity_gb={}, block_size_mb={}, expected_items={}, "
                     "false_positive_rate={}, bits_per_element={}, total_memory={} bytes",
                     config.getEdgeCapacityGB(), config.getBlockSizeMB(), num_items,
                     fpr, bits_per_elem, total_bytes);
        
        bloom_filter_ = elastic_rose::CountingBloomFilter(total_bytes, fpr, 0);
    }

    void add(uint32_t datastreamID, uint32_t blockId) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        bloom_filter_.Add(std::to_string(datastreamID) + ":" + std::to_string(blockId));
    }

    void remove(uint32_t datastreamID, uint32_t blockId) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        bloom_filter_.Remove(std::to_string(datastreamID) + ":" + std::to_string(blockId));
    }

    std::vector<uint32_t> range_query(uint32_t datastreamID,
        const uint32_t start_blockId, const uint32_t end_blockId) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        std::vector<uint32_t> result;
        for (uint32_t blockId = start_blockId; blockId <= end_blockId; blockId++) {
            std::string key = std::to_string(datastreamID) + ":" + std::to_string(blockId);
            if (bloom_filter_.KeyMayMatch(key)) {
                result.push_back(blockId);
            }
        }
        return result;
    }

    size_t memory_usage() const override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        // 布隆过滤器的内存使用 = 计数位数组的大小
        return bloom_filter_.getMemoryUsage();
    }
};

struct MixIndex : public Common::BaseIndex {
    mutable std::shared_mutex mutex_;
    std::unordered_map<uint32_t, roaring::Roaring> index_;
    MixIndex() {}

    void add(uint32_t datastreamID, uint32_t blockId) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        index_[datastreamID].add(blockId);
    }
    
    void remove(uint32_t datastreamID, uint32_t blockId) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto it = index_.find(datastreamID);
        if (it != index_.end()) {
            it->second.remove(blockId);
        }
    }
    
    std::vector<uint32_t> range_query(uint32_t datastreamID,
        const uint32_t start_blockId, const uint32_t end_blockId) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        std::vector<uint32_t> result;
        auto it = index_.find(datastreamID);
        if (it != index_.end()) {
            roaring::Roaring range_bitmap;
            range_bitmap.addRange(start_blockId, end_blockId + 1);
            roaring::Roaring result_bitmap = it->second & range_bitmap;
            result.reserve(result_bitmap.cardinality());
            for(uint32_t value : result_bitmap) {
                result.push_back(value);
            }
        }
        return result;
    }

    size_t memory_usage() const override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        size_t total_bytes = 0;
        
        // 基础哈希表开销
        total_bytes += sizeof(std::unordered_map<uint32_t, roaring::Roaring>);
        
        // 遍历所有位图计算总内存使用
        for (const auto& pair : index_) {
            // 键的开销
            total_bytes += sizeof(uint32_t);
            
            // 获取实际运行时内存统计
            roaring::api::roaring_statistics_t stats;
            roaring::api::roaring_bitmap_statistics(&pair.second.roaring, &stats);
            
            // 累加所有容器的实际内存使用
            total_bytes += stats.n_bytes_array_containers;
            total_bytes += stats.n_bytes_run_containers;
            total_bytes += stats.n_bytes_bitset_containers;
        }
        
        return total_bytes;
    }
};

class EdgeCacheIndex {
private:
    // 节点ID -> [数据源ID -> 压缩位图]
    std::unordered_map<std::string, std::unique_ptr<Common::BaseIndex>> timeseries_main_index_;
    // 添加一个枚举来指定索引类型
    enum class IndexType {
        MIX_INDEX,
        BLOOM_FILTER
    };
    std::unordered_map<std::string, IndexType> index_type_;
    std::unordered_map<std::string, int64_t> node_latencies_;
    int64_t config_latency_threshold_ms_ = 30; // 默认阈值为30ms

    std::unique_ptr<Common::BaseIndex> createIndex(const std::string& nodeId);

public:
    EdgeCacheIndex() {}
    
    std::vector<std::string> queryIndex(const std::string& dataKey) const;

    tbb::concurrent_hash_map<uint32_t, std::string>& queryMainIndex(const std::string& datastream_id, const uint32_t start_blockId, 
        const uint32_t end_blockId, const uint32_t stream_uniqueId);

    void setNodeLatency(const std::string& nodeId, int64_t latency);
    void setLatencyThreshold(int64_t threshold);

    int64_t getNodeLatency(const std::string& nodeId) const;
    void addBlock(uint32_t block_id,
                 const std::string& node_id,
                 uint32_t stream_unique_id);
                 
    void removeBlock(uint32_t block_id,
                    const std::string& node_id,
                    uint32_t stream_unique_id);

    // 获取所有索引的总内存使用
    size_t totalMemoryUsage() const {
        size_t total_bytes = 0;
        for (const auto& pair : timeseries_main_index_) {
            total_bytes += pair.second->memory_usage();
        }
        return total_bytes;
    }

    size_t getSingleIndexMemoryUsage(const std::string& nodeId) const {
        if (timeseries_main_index_.find(nodeId) == timeseries_main_index_.end()) {
            spdlog::error("Attempting to get memory usage for non-existent node: {}", nodeId);
            return 0;
        }
        return timeseries_main_index_.at(nodeId)->memory_usage();
    }
};

#endif // EDGE_CACHE_INDEX_HPP
