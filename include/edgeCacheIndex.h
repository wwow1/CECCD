#ifndef EDGE_CACHE_INDEX_HPP
#define EDGE_CACHE_INDEX_HPP

#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <iostream>
#include "../include/rosetta.hpp"
#include "../include/common.h"
#include "roaring.hh"
#include "roaring.h"
#include <postgresql/libpq-fe.h>

class EdgeCacheIndex {
private:
    struct TableRosetta {
        std::unordered_map< std::string, elastic_rose::Rosetta > rosetta_index_;
    };
    std::string nodeId_;  // 当前节点ID
     // 邻居Id -> <数据源ID, 压缩位图>
     // 压缩位图中保存的是时间点
    Common::TableSchema schema_;
    std::unordered_map< std::string, std::unordered_map< std::string, roaring::Roaring64Map > > timeseries_main_index_;
    std::unordered_map< std::string, std::unordered_map< std::string, TableRosetta > > fields_index_;

public:
    EdgeCacheIndex(const std::string& id, Common::TableSchema schema) : nodeId_(id) {
        // TODO(zhengfuyu) : rosetta初始化
    }

    void updateIndex(const uint64_t timeseries_key, const std::string& src_monitor_id, 
        const std::string& neighbor_nodeId, PGresult* migrate_data);

    void removeFromIndex(const uint64_t timeseries_key, const std::string& src_monitor_id, 
        const std::string& neighbor_nodeId, PGresult* migrate_data);

    std::vector<std::string> queryIndex(const std::string& dataKey) const;

    // void printIndex() const {
    //     std::cout << "EdgeCacheIndex (Node ID: " << nodeId << ")\n";
    //     for (const auto& [dataKey, neighbors] : neighborIndex) {
    //         std::cout << "  Data Key: " << dataKey << " -> [";
    //         for (size_t i = 0; i < neighbors.size(); ++i) {
    //             std::cout << neighbors[i];
    //             if (i < neighbors.size() - 1) std::cout << ", ";
    //         }
    //         std::cout << "]\n";
    //     }
    // }
};

#endif // EDGE_CACHE_INDEX_HPP
