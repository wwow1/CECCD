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

struct MyBloomFilter : public Common::BaseIndex {
    elastic_rose::CountingBloomFilter bloom_filter_;
    void add(uint32_t datastreamID, uint32_t blockId) {
        bloom_filter_.Add(datastreamID + ":" + std::to_string(blockId));
    }
    void remove(uint32_t datastreamID, uint32_t blockId) {
        bloom_filter_.Remove(datastreamID + ":" + std::to_string(blockId));
    }
    std::vector<uint32_t> range_query(uint32_t datastreamID,
        const uint32_t start_blockId, const uint32_t end_blockId) const {
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
    // datastreamID-> blockID
    std::unordered_map< uint32_t, roaring::Roaring > index_;
    void add(uint32_t datastreamID, uint32_t blockId) {
        index_[datastreamID].add(blockId);
    }
    void remove(uint32_t datastreamID, uint32_t blockId) {
        index_[datastreamID].remove(blockId);
    }
    // [start_blockId, end_blockId]
    std::vector<uint32_t> range_query(uint32_t datastreamID,
        const uint32_t start_blockId, const uint32_t end_blockId) const {
        std::vector<uint32_t> result;
        roaring::Roaring range_bitmap;
        range_bitmap.addRange(start_blockId, end_blockId + 1); // 创建范围
        auto it = index_.find(datastreamID);
        if (it != index_.end()) {
            roaring::Roaring result_bitmap = it->second & range_bitmap;
            // 直接获取结果位图中的所有值
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
    // 数据源名字 -> 数据源元数据
    std::unordered_map< std::string, Common::StreamMeta > schema_;
    // 节点ID -> [数据源ID -> 压缩位图]
    std::unordered_map<std::string, std::unique_ptr<Common::BaseIndex>> timeseries_main_index_;
    // 添加一个枚举来指定索引类型
    enum class IndexType {
        MIX_INDEX,
        BLOOM_FILTER
    };
    std::unordered_map<std::string, IndexType> index_type_;

public:
    EdgeCacheIndex() {}

    uint32_t getBlockId(uint64_t block_start_time, std::string datastream_id);

    void updateIndex(const std::string& neighbor_nodeId, const cloud_edge_cache::Metadata& meta);

    void updateStreamMeta(const std::vector<Common::StreamMeta> stream_metas);

    void removeFromIndex(const std::string& neighbor_nodeId, const cloud_edge_cache::Metadata& meta);

    std::vector<std::string> queryIndex(const std::string& dataKey) const;
    tbb::concurrent_hash_map<uint32_t, std::string>& queryMainIndex(const std::string& datastream_id, const uint64_t& timeseries_start, 
        const uint64_t& timeseries_end);
};

#endif // EDGE_CACHE_INDEX_HPP
