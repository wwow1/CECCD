#ifndef EDGE_CACHE_INDEX_HPP
#define EDGE_CACHE_INDEX_HPP

#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <iostream>
#include "../include/rosetta.hpp"

class EdgeCacheIndex {
private:
    std::string nodeId;  // 当前节点ID
    std::unordered_map<std::string, std::vector<std::string>> neighborIndex;  // 邻居缓存索引

public:
    EdgeCacheIndex(const std::string& id) : nodeId(id) {}
    void updateIndex(const std::string& dataKey, const std::string& neighborNodeId);

    void removeFromIndex(const std::string& dataKey, const std::string& neighborNodeId);

    std::vector<std::string> queryIndex(const std::string& dataKey) const;

    void printIndex() const {
        std::cout << "EdgeCacheIndex (Node ID: " << nodeId << ")\n";
        for (const auto& [dataKey, neighbors] : neighborIndex) {
            std::cout << "  Data Key: " << dataKey << " -> [";
            for (size_t i = 0; i < neighbors.size(); ++i) {
                std::cout << neighbors[i];
                if (i < neighbors.size() - 1) std::cout << ", ";
            }
            std::cout << "]\n";
        }
    }
};

#endif // EDGE_CACHE_INDEX_HPP
