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
        
        size_t max_blocks = (config.getEdgeCapacityGB() * 1024) / config.getBlockSizeMB();
        double fpr = config.getBloomFilterFPR();
        
        spdlog::info("Initializing BloomFilter: edge_capacity_gb={}, block_size_mb={}, expected_items={}, "
                     "false_positive_rate={}",
                     config.getEdgeCapacityGB(), config.getBlockSizeMB(), max_blocks, fpr);
        
        bloom_filter_ = elastic_rose::CountingBloomFilter(max_blocks, fpr);
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
        // 底层默认使用8位计数器，但是我们计算的是4位计数器的情况
        return bloom_filter_.getMemoryUsage() * 3 / 8;
    }
};

struct MixIndex : public Common::BaseIndex {
    mutable std::shared_mutex mutex_;
    std::unordered_map<uint32_t, roaring::Roaring> index_;
    MixIndex() {}

    void compress() {
        for(auto &it : index_) {
            bool compressed = it.second.runOptimize();
        }
    }

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
            total_bytes += stats.n_bytes_array_containers * 2;
            total_bytes += stats.n_bytes_run_containers * 2;
            total_bytes += stats.n_bytes_bitset_containers * 2;
        }
        spdlog::info("single MixIndex memory usage: {}", total_bytes);
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

    void compress() {
        for (auto &[node_name, type] : index_type_) {
            if (type == IndexType::MIX_INDEX) {
                auto mix_index = dynamic_cast<MixIndex*>(timeseries_main_index_[node_name].get());
                mix_index->compress();
            } else {
                spdlog::info("bloom doesn't need to compress");
            }
        }
    }

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
