#ifndef COMMON_H
#define COMMON_H

#include <string>
#include <vector>
#include <map>
#include <iostream>

namespace Common {
// 定义一些常用的类型映射或枚举（可选）

struct UniqueKey {
    std::string table_name_;
    std::string source_id_;
    uint64_t timestamp_;
};

enum class DataType {
    INT,
    STRING,
    DOUBLE,
    BOOL
};

// 定义一个通用的数据库表列结构
struct Column {
    std::string name;    // 列名
    DataType type;    // 数据类型
    bool isPrimaryKey;   // 是否为主键
    bool isNullable;     // 是否允许为空

    // 构造函数
    Column(const std::string& name, DataType type, bool isPrimaryKey = false, bool isNullable = true)
        : name(name), type(type), isPrimaryKey(isPrimaryKey), isNullable(isNullable) {}
};

// 定义一个通用的每个数据流的最小表结构
struct StreamMeta {
    std::string datastream_id_;
    uint32_t unique_id_;
    uint64_t start_time_;
    uint32_t time_range_;
    uint32_t block_size_;
};

// struct BlockInfo {
//     uint32_t block_id;
//     uint32_t datastream_unique_id;
//     int64_t start_timestamp;
//     int64_t end_timestamp;
    
//     BlockInfo() = default;
//     BlockInfo(uint32_t bid, uint32_t did, int64_t start, int64_t end)
//         : block_id(bid)
//         , datastream_id(did)
//         , start_timestamp(start)
//         , end_timestamp(end) {}
// };

class BaseIndex {
    public:
    virtual ~BaseIndex() = default;
    virtual void add(uint32_t datastreamID, uint32_t blockId) = 0; // 纯虚函数
    virtual void remove(uint32_t datastreamID, uint32_t blockId ) = 0; // 纯虚函数
    virtual std::vector<uint32_t> range_query(uint32_t datastreamID, uint32_t start_blockId, uint32_t end_blockId) const = 0; // 纯虚函数
};

} // namespace Common

#endif // COMMON_H
