#include "../include/edgeCacheIndex.h"

void EdgeCacheIndex::updateIndex(const std::string& dataKey, const std::string& neighborNodeId) {
    auto& neighbors = neighborIndex[dataKey];
    if (std::find(neighbors.begin(), neighbors.end(), neighborNodeId) == neighbors.end()) {
        neighbors.push_back(neighborNodeId);
    }
}

void EdgeCacheIndex::removeFromIndex(const std::string& dataKey, const std::string& neighborNodeId) {
    auto it = neighborIndex.find(dataKey);
    if (it != neighborIndex.end()) {
        auto& neighbors = it->second;
        neighbors.erase(std::remove(neighbors.begin(), neighbors.end(), neighborNodeId), neighbors.end());
        if (neighbors.empty()) {
            neighborIndex.erase(it);
        }
    }
}

std::vector<std::string> EdgeCacheIndex::queryIndex(const std::string& dataKey) const {
    auto it = neighborIndex.find(dataKey);
    if (it != neighborIndex.end()) {
        return it->second;
    }
    return {};
}