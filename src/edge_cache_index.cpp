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

void EdgeCacheIndex::addBlock(uint32_t block_id,
                             const std::string& node_id,
                             uint32_t stream_unique_id) {
    // 如果该节点还没有索引，创建一个新的索引
    if (timeseries_main_index_.find(node_id) == timeseries_main_index_.end()) {
        timeseries_main_index_[node_id] = createIndex(node_id);
    }
    
    // 将数据块添加到索引中
    auto& local_index = timeseries_main_index_[node_id];
    local_index->add(stream_unique_id, block_id);
}

void EdgeCacheIndex::removeBlock(uint32_t block_id,
                                 const std::string& node_id,
                                 uint32_t stream_unique_id) {
    // 如果该节点的索引不存在，直接返回
    auto it = timeseries_main_index_.find(node_id);
    if (it == timeseries_main_index_.end()) {
        return;
    }
    
    // 从索引中移除数据块
    auto& local_index = it->second;
    local_index->remove(stream_unique_id, block_id);
}