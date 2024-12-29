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

struct MyBloomFilter : public Common::BaseIndex {
    mutable std::shared_mutex mutex_;
    elastic_rose::CountingBloomFilter bloom_filter_;
    void add(uint32_t datastreamID, uint32_t blockId) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        bloom_filter_.Add(datastreamID + ":" + std::to_string(blockId));
    }
    void remove(uint32_t datastreamID, uint32_t blockId) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        bloom_filter_.Remove(datastreamID + ":" + std::to_string(blockId));
    }
    std::vector<uint32_t> range_query(uint32_t datastreamID,
        const uint32_t start_blockId, const uint32_t end_blockId) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        std::vector<uint32_t> result;
        for (uint32_t blockId = start_blockId; blockId <= end_blockId; blockId++) {
            if (bloom_filter_.KeyMayMatch(datastreamID + ":" + std::to_string(blockId))) {
                result.push_back(blockId);
            }
        }
        return result;
    }
};

struct MixIndex : public Common::BaseIndex {
    mutable std::shared_mutex mutex_;
    std::unordered_map<uint32_t, roaring::Roaring> index_;
    
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
};

class EdgeCacheIndex {
private:
    // 节点ID -> [数据源ID -> 压缩位图]
    std::unordered_map<std::string, std::unique_ptr<Common::BaseIndex>> timeseries_main_index_;
    // 添加一个枚举来指定索引��型
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

    void addBlock(uint32_t block_id,
                 const std::string& node_id,
                 uint32_t stream_unique_id);
                 
    void removeBlock(uint32_t block_id,
                    const std::string& node_id,
                    uint32_t stream_unique_id);
};

#endif // EDGE_CACHE_INDEX_HPP
