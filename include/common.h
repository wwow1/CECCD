#ifndef COMMON_H
#define COMMON_H

#include <string>
#include <vector>
#include <map>
#include <iostream>

namespace Common {
// 定义一些常用的类型映射或枚举（可选）

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

// 定义一个通用的数据库表 Schema 结构
class TableSchema {
public:
    std::vector<Column> columns;
    // 默认构造函数
    TableSchema() = default;

    // 赋值运算符重载
    TableSchema& operator=(const TableSchema& other) {
        if (this != &other) { // 防止自赋值
            columns.clear();  // 清除现有数据
            copyFrom(other);  // 复制数据
        }
        return *this;
    }

    // 自定义复制函数
    void copyFrom(const TableSchema& other) {
        columns = other.columns;
    }

    // 拷贝构造函数
    TableSchema(const TableSchema& other) {
        copyFrom(other);
    }
    
    // 添加列的方法
    void addColumn(const std::string& name, const std::string& type, bool isPrimaryKey = false, bool isNullable = true) {
        columns.emplace_back(name, type, isPrimaryKey, isNullable);
    }

    // 打印 Schema 信息的方法
    void printSchema() const {
        std::cout << "Table Schema:\n";
        for (const auto& col : columns) {
            std::cout << "Column: " << col.name
                      << ", Primary Key: " << (col.isPrimaryKey ? "Yes" : "No")
                      << ", Nullable: " << (col.isNullable ? "Yes" : "No")
                      << std::endl;
        }
    }
};

} // namespace Common

#endif // COMMON_H
