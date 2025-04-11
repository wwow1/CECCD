#include "edge_cache_index.h"

#include <bits/types/FILE.h>
#include <cstdint>

std::unique_ptr<Common::BaseIndex> EdgeCacheIndex::createIndex(const std::string& nodeId) {
    spdlog::info("createIndex: nodeId={}, latency={}ms, threshold={}ms", nodeId, node_latencies_[nodeId], config_latency_threshold_ms_);
    if (node_latencies_[nodeId] >= config_latency_threshold_ms_) {
        spdlog::info("Using MixIndex for node {} (latency: {}ms)", nodeId, node_latencies_[nodeId]);
        index_type_[nodeId] = IndexType::MIX_INDEX;
        return std::make_unique<MixIndex>();
    } else {
        spdlog::info("Using BloomFilter for node {} (latency: {}ms)", nodeId, node_latencies_[nodeId]);
        index_type_[nodeId] = IndexType::BLOOM_FILTER;
        return std::make_unique<MyBloomFilter>();
    }
}

void EdgeCacheIndex::setNodeLatency(const std::string& nodeId, double latency) {
    node_latencies_[nodeId] = latency;
}

double EdgeCacheIndex::getNodeLatency(const std::string& nodeId) const {
    if (node_latencies_.find(nodeId) == node_latencies_.end()) {
        spdlog::error("Attempting to get latency for non-existent node: {}", nodeId);
        // 返回一个默认值或抛出更明确的异常
        return -1;
    }
    return node_latencies_.at(nodeId);
}

void EdgeCacheIndex::setLatencyThreshold(int64_t threshold) {
    config_latency_threshold_ms_ = threshold;
    spdlog::info("setLatencyThreshold: {}", threshold);
}

// 目前只支持单个数据源的查询，TODO（支持多个数据源的查询）
// 查询主索引找出所有包含该数据源的节点
// 返回值: 块ID -> 节点ID
tbb::concurrent_hash_map<uint32_t, std::string>& EdgeCacheIndex::queryMainIndex(const std::string& datastream_id, const uint32_t start_blockId, 
    const uint32_t end_blockId, const uint32_t stream_uniqueId) {
    static tbb::concurrent_hash_map<uint32_t, std::string> results;
    results.clear();
    // std::cout << "queryMainIndex start" << start_blockId << " end " << end_blockId << std::endl;
    #pragma omp parallel for
    for(const auto& [neighbor_nodeId, index] : timeseries_main_index_) {
        // std::cout << "queryMainIndex neighbor_nodeId " << neighbor_nodeId << std::endl;
        std::vector<uint32_t> matched_blocks = index->range_query(
            stream_uniqueId, 
            start_blockId, 
            end_blockId
        );
        
        for(const auto& block_id : matched_blocks) {
            typename tbb::concurrent_hash_map<uint32_t, std::string>::accessor accessor;
            bool inserted = results.insert(accessor, block_id);
            
            // 如果插入成功，直接设置为当前的 neighbor_nodeId
            if (inserted) {
                accessor->second = neighbor_nodeId;
            } else {
                // 如果已经存在，比较延迟
                int64_t current_latency = getNodeLatency(neighbor_nodeId);
                int64_t existing_latency = getNodeLatency(accessor->second);
                
                // 选择延迟更低的 neighbor_nodeId
                if (current_latency < existing_latency) {
                    accessor->second = neighbor_nodeId;
                }
            }
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