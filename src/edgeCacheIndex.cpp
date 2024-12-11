#include "../include/edgeCacheIndex.h"

#include <bits/types/FILE.h>
#include <cstdint>

void EdgeCacheIndex::updateIndex(const uint64_t timeseries_key, const std::string& src_monitor_id, 
        const std::string& neighbor_nodeId, PGresult* migrate_data) {
    auto roaring_map = timeseries_main_index_[neighbor_nodeId][src_monitor_id];
    // 添加主键
    roaring_map.add(timeseries_key);
    // // 解析每一行数据
    // int nfields = PQnfields(migrate_data); // 获取列数
    // // 遍历每个列
    // int nrows = PQntuples(migrate_data);
    // for (int row = 0; row < nrows; ++row) {
    //     for (int i = 0; i < nfields; ++i) {
    //         const char *column_name = PQfname(migrate_data, i);  // 获取列名

    //         const char *value = PQgetvalue(migrate_data, row, i); // 获取字段的值
    //         char *endptr;  // 结束指针
    //         uint64_t v64 = strtoull(value, &endptr, 10);  // 10 表示基数为 10

    //         auto table_rosetta = (fields_index_[neighbor_nodeId][src_monitor_id]).rosetta_index_;
    //         if (table_rosetta.find(column_name) == table_rosetta.end()) continue;
    //         table_rosetta[column_name].insertKey(v64);
    //     }
    // }
}

void EdgeCacheIndex::removeFromIndex(const uint64_t timeseries_key, const std::string& src_monitor_id, 
        const std::string& neighbor_nodeId, PGresult* migrate_data) {
    auto roaring_map = timeseries_main_index_[neighbor_nodeId][src_monitor_id];
    // 添加主键
    roaring_map.remove(timeseries_key);
    // // 解析每一行数据
    // int nfields = PQnfields(migrate_data); // 获取列数
    // // 遍历每个列
    // int nrows = PQntuples(migrate_data);
    // for (int row = 0; row < nrows; ++row) {
    //     for (int i = 0; i < nfields; ++i) {
    //         const char *column_name = PQfname(migrate_data, i);  // 获取列名

    //         const char *value = PQgetvalue(migrate_data, row, i); // 获取字段的值
    //         char *endptr;  // 结束指针
    //         uint64_t v64 = strtoull(value, &endptr, 10);  // 10 表示基数为 10

    //         auto table_rosetta = (fields_index_[neighbor_nodeId][src_monitor_id]).rosetta_index_;
    //         if (table_rosetta.find(column_name) == table_rosetta.end()) continue;
    //         table_rosetta[column_name].DeleteKey(v64);
    //     }
    // }
}

template <typename T>
bool compare(const T& left, const T& right, const std::string& op) {
    if (op == ">") return left > right;
    if (op == "<") return left < right;
    if (op == "==") return left == right;
    if (op == ">=") return left >= right;
    if (op == "<=") return left <= right;
    return false;
}

tbb::concurrent_hash_map<uint64_t, std::string>& EdgeCacheIndex::queryMainIndex(const std::string& src_monitor_id, const uint64_t& timeseries_start, 
    const uint64_t& timeseries_end) {
    tbb::concurrent_hash_map<uint64_t, std::string> task_allocation;

    // 使用 OpenMP 并行化外层节点遍历
    #pragma omp parallel for
    for (auto node_mp = timeseries_main_index_[src_monitor_id].begin(); node_mp != timeseries_main_index_[src_monitor_id].end(); ++node_mp) {
        const std::string& node_id = node_mp->first;
        const auto& source_map = node_mp->second;
        // 获取范围 [x, y) 中的交集
        roaring::Roaring64Map range_bitmap;
        range_bitmap.addRange(timeseries_start, timeseries_end); // 创建范围
        roaring::Roaring64Map result_bitmap = source_map & range_bitmap;

        auto it = range_bitmap.begin();
        bool exist = it.move(timeseries_start);
        // 遍历结果并分配任务
        while (*it <= timeseries_end) {
            task_allocation.insert({*it, node_id}); // 分配任务
        }
        return task_allocation;
    }

    // // 输出分配结果（调试用）
    // for (const auto& [key, value] : task_allocation) {
    //     std::cout << "Data Point " << key << " assigned to Node " << value.first
    //               << ", Source " << value.second << std::endl;
    // }
}

// std::vector<std::string> EdgeCacheIndex::queryRosettaIndex(const std::string& src_monitor_id, const std::string& column, const char* value, const std::string& op) const {
//     auto it = neighborIndex.find(dataKey);
//     if (it != neighborIndex.end()) {
//         return it->second;
//     }
//     return {};
// }