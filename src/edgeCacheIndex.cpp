#include "edgeCacheIndex.h"

#include <bits/types/FILE.h>
#include <cstdint>

uint32_t EdgeCacheIndex::getBlockId(uint64_t block_start_time, std::string datastream_id) {
    auto& source_schema = schema_[datastream_id];
    uint32_t blockId = (block_start_time - source_schema.start_time_) / source_schema.time_range_;
    return blockId;
}

void EdgeCacheIndex::updateIndex(const std::string& neighbor_nodeId, const cloud_edge_cache::Metadata& meta) {
    if (timeseries_main_index_.find(neighbor_nodeId) == timeseries_main_index_.end()) {
        // TODO(zhengfuyu)： 基于我们的延迟阈值来决定使用哪种索引
        timeseries_main_index_[neighbor_nodeId] = createIndex();
    }
    
    auto& local_index = timeseries_main_index_[neighbor_nodeId];
    uint32_t blockId = getBlockId(meta.timestamp(), meta.source_id());
    uint32_t stream_uniqueId = schema_[meta.source_id()].unique_id_;
    local_index->add(stream_uniqueId, blockId);
}

void EdgeCacheIndex::removeFromIndex(const std::string& neighbor_nodeId, const cloud_edge_cache::Metadata& meta) {
    if (timeseries_main_index_.find(neighbor_nodeId) == timeseries_main_index_.end()) {
        return;
    }
    
    auto& local_index = timeseries_main_index_[neighbor_nodeId];
    uint32_t blockId = getBlockId(meta.timestamp(), meta.source_id());
    uint32_t stream_uniqueId = schema_[meta.source_id()].unique_id_;
    local_index->remove(stream_uniqueId, blockId);
}

void EdgeCacheIndex::updateStreamMeta(const std::vector<Common::StreamMeta> stream_metas) {
    for (auto &meta : stream_metas) {
        if (schema_.find(meta.datastream_id_) != schema_.end()) continue;
        schema_[meta.datastream_id_] = meta;
    }
}

// 查询主索引找出所有包含该数据源的节点
// 返回值: 块ID -> 节点ID
tbb::concurrent_hash_map<uint32_t, std::string>& EdgeCacheIndex::queryMainIndex(const std::string& datastream_id, const uint64_t& timeseries_start, 
    const uint64_t& timeseries_end) {
    uint32_t start_blockId = getBlockId(timeseries_start, datastream_id);
    uint32_t end_blockId = getBlockId(timeseries_end, datastream_id);
    uint32_t stream_uniqueId = schema_[datastream_id].unique_id_;
    
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
