#include "edge_cache_index.h"

#include <bits/types/FILE.h>
#include <cstdint>

std::unique_ptr<Common::BaseIndex> EdgeCacheIndex::createIndex(const std::string& nodeId) {
    if (node_latencies_[nodeId] > config_latency_threshold_ms_) {
        std::cout << "Using MixIndex for node " << nodeId << " (latency: " 
                  << node_latencies_[nodeId] << "ms)" << std::endl;
        return std::make_unique<MixIndex>();
    } else {
        std::cout << "Using BloomFilter for node " << nodeId << " (latency: " 
                  << node_latencies_[nodeId] << "ms)" << std::endl;
        return std::make_unique<MyBloomFilter>();
    }
}

void EdgeCacheIndex::setNodeLatency(const std::string& nodeId, int64_t latency) {
    node_latencies_[nodeId] = latency;
}

void EdgeCacheIndex::setLatencyThreshold(int64_t threshold) {
    config_latency_threshold_ms_ = threshold;
}

void EdgeCacheIndex::updateIndex(const std::string& neighbor_nodeId, const cloud_edge_cache::Metadata& meta) {
    // if (timeseries_main_index_.find(neighbor_nodeId) == timeseries_main_index_.end()) {
    //     timeseries_main_index_[neighbor_nodeId] = createIndex(neighbor_nodeId);
    // }
    
    // auto& local_index = timeseries_main_index_[neighbor_nodeId];
    // uint32_t blockId = getBlockId(meta.timestamp(), meta.source_id());
    // uint32_t stream_uniqueId = schema_[meta.source_id()].unique_id_;
    // local_index->add(stream_uniqueId, blockId);
}

void EdgeCacheIndex::removeFromIndex(const std::string& neighbor_nodeId, const cloud_edge_cache::Metadata& meta) {
    // if (timeseries_main_index_.find(neighbor_nodeId) == timeseries_main_index_.end()) {
    //     return;
    // }
    
    // auto& local_index = timeseries_main_index_[neighbor_nodeId];
    // uint32_t blockId = getBlockId(meta.timestamp(), meta.source_id());
    // uint32_t stream_uniqueId = schema_[meta.source_id()].unique_id_;
    // local_index->remove(stream_uniqueId, blockId);
}

// void EdgeCacheIndex::updateStreamMeta(const std::vector<Common::StreamMeta> stream_metas) {
//     for (auto &meta : stream_metas) {
//         if (schema_.find(meta.datastream_id_) != schema_.end()) continue;
//         schema_[meta.datastream_id_] = meta;
//     }
// }

// 目前只支持单个数据源的查询，TODO（支持多个数据源的查询）
// 查询主索引找出所有包含该数据源的节点
// 返回值: 块ID -> 节点ID
tbb::concurrent_hash_map<uint32_t, std::string>& EdgeCacheIndex::queryMainIndex(const std::string& datastream_id, const uint32_t start_blockId, 
    const uint32_t end_blockId, const uint32_t stream_uniqueId) {
    static tbb::concurrent_hash_map<uint32_t, std::string> results;
    results.clear();
    
    #pragma omp parallel for
    for(const auto& [neighbor_nodeId, index] : timeseries_main_index_) {
        std::vector<uint32_t> matched_blocks = index->range_query(
            stream_uniqueId, 
            start_blockId, 
            end_blockId
        );
        
        for(const auto& block_id : matched_blocks) {
            typename tbb::concurrent_hash_map<uint32_t, std::string>::accessor accessor;
            results.insert(accessor, block_id);
            accessor->second = neighbor_nodeId;
        }
    }
    
    return results;
}