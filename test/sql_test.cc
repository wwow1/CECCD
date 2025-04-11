// test_client.cpp
#include <iostream>
#include "../include/client.h"
#include <SQLParser.h>

// 辅助函数来解析 WHERE 子句
void parseWhereClause(const hsql::Expr* expr, 
                                int64_t& start_timestamp, 
                                int64_t& end_timestamp) {
    if (!expr) return;
    
    if (expr->type == hsql::kExprOperator) {
        if (expr->opType == hsql::kOpAnd) {
            // 递归处理 AND 条件的两边
            parseWhereClause(expr->expr, start_timestamp, end_timestamp);
            parseWhereClause(expr->expr2, start_timestamp, end_timestamp);
        } else if (expr->opType == hsql::kOpOr) {
            // OR 条件可能会使时间范围失效，这里选择保持最宽松的范围
            std::cout << "Warning: OR condition detected, using widest possible time range" << std::endl;
            start_timestamp = std::numeric_limits<int64_t>::min();
            end_timestamp = std::numeric_limits<int64_t>::max();
        } else {
            // 处理比较操作符
            if (expr->expr && expr->expr->type == hsql::kExprColumnRef) {
                std::cout << std::string(expr->expr->name) << std::endl;
            }
            if (expr->expr && expr->expr->type == hsql::kExprColumnRef &&
                std::string(expr->expr->name) == "date_time") {
                if (expr->expr2 && expr->expr2->type == hsql::kExprLiteralInt) {
                    switch (expr->opType) {
                        case hsql::kOpGreaterEq:
                            start_timestamp = std::max(start_timestamp, expr->expr2->ival);
                            break;
                        case hsql::kOpGreater:
                            start_timestamp = std::max(start_timestamp, expr->expr2->ival + 1);
                            break;
                        case hsql::kOpLessEq:
                            end_timestamp = std::min(end_timestamp, expr->expr2->ival);
                            break;
                        case hsql::kOpLess:
                            end_timestamp = std::min(end_timestamp, expr->expr2->ival - 1);
                            break;
                    }
                }
            }
        }
    }
}

std::tuple<std::string, int64_t, int64_t> parseSQLQuery(const std::string& sql_query) {
    hsql::SQLParserResult result;
    bool success = hsql::SQLParser::parse(sql_query, &result);
    
    int64_t start_timestamp = std::numeric_limits<int64_t>::min();
    int64_t end_timestamp = std::numeric_limits<int64_t>::max();
    std::string table_name;

    if (result.isValid()) {
        const hsql::SQLStatement* stmt = result.getStatement(0);
        if (stmt->type() == hsql::kStmtSelect) {
            const hsql::SelectStatement* select = (const hsql::SelectStatement*) stmt;
            if (select->fromTable) {
                table_name = select->fromTable->getName();
                // std::cout << "EdgeServer::parseSQLQuery table_name: " << select->fromTable->getName() << std::endl;
            }
            if (select->whereClause != nullptr) {
                parseWhereClause(select->whereClause, start_timestamp, end_timestamp);
                // std::cout << "EdgeServer::parseSQLQuery start_time " << start_timestamp << " end_time " << end_timestamp << std::endl;
            }
        }
    } else {
        std::cerr << "SQL query parsing failed." << std::endl;
    }

    return {table_name, start_timestamp, end_timestamp};
}

#include <random>
#include <vector>

// 生成随机查询语句的函数
std::string generateRandomQuery(int64_t min_time, int64_t max_time, int64_t time_unit) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis_field(0, 19);  // 随机选择字段
    std::uniform_int_distribution<> dis_num_fields(1, 2);  // 随机选择 1 或 2 个字段
    std::uniform_int_distribution<> dis_aggregation(0, 9);  // 概率性选择聚合语句，10% 使用聚合函数
    std::uniform_int_distribution<> dis_start_time(0, (max_time - min_time) / time_unit);  // 随机选择起始时间
    std::uniform_int_distribution<> dis_duration(1, (max_time - min_time) / time_unit - dis_start_time(gen));  // 随机选择持续时间
    std::uniform_int_distribution<> dis_filter(0, 3); // 随机选择过滤函数

    std::vector<std::string> fields = {
        "s_0", "s_1", "s_2", "s_3", "s_4", "s_5",
        "s_6", "s_7", "s_8", "s_9", "s_10", "s_11", "s_12", "s_13", "s_14",
        "s_15", "s_16", "s_17", "s_18", "s_19"
    };

    std::string query = "SELECT ";

    // 随机选择一个或两个字段
    int num_fields = dis_num_fields(gen);
    for (int i = 0; i < num_fields; ++i) {
        if (i > 0) query += ", ";
        int field_index = dis_field(gen);
        std::string field = fields[field_index];

        // 概率性地追加聚合语句，10% 使用聚合函数
        int aggregation = dis_aggregation(gen);
        switch (aggregation) {
            case 1:
                query += "MAX(" + field + ")";
                break;
            case 2:
                query += "MIN(" + field + ")";
                break;
            case 3:
                query += "AVG(" + field + ")";
                break;
            case 4:
                query += "SUM(" + field + ")";
                break;
            case 5:
                query += "COUNT(" + field + ")";
                break;
            default:
                query += field;
                break;
        }

        // 追加随机过滤函数
        int filter = dis_filter(gen);
        switch (filter) {
            case 0:
                query += " > 0";
                break;
            case 1:
                query += " < 100";
                break;
            case 2:
                query += " IS NOT NULL";
                break;
            case 3:
                query += " = 50";
                break;
        }
    }

    query += " FROM postgres";

    // 随机选择起始时间和持续时间
    int64_t start_time = min_time + dis_start_time(gen) * time_unit;
    int64_t end_time = start_time + dis_duration(gen) * time_unit;

    // 添加 date_time 过滤条件
    query += " WHERE date_time >= " + std::to_string(start_time) + " AND date_time <= " + std::to_string(end_time);

    return query;
}

// 修改 main 函数以使用生成的随机查询
int main(int argc, char* argv[]) {
    // 检查参数数量
    if (argc != 5) {
        std::cerr << "Usage: " << argv[0] << " <server_address> <port> <min_time> <max_time>" << std::endl;
        return 1;
    }

    // 创建 Client 实例，连接到服务器
    std::string server_address = std::string(argv[1]) + ":" + argv[2];
    std::cout << " server_address " << server_address << std::endl;
    Client client(server_address); // 使用参数提供的服务器地址和端口

    // 获取时间限制
    int64_t min_time = std::stoll(argv[3]);
    int64_t max_time = std::stoll(argv[4]);
    int64_t time_unit = 10000; // 时间单元

    // 生成随机查询语句
    std::string random_query = generateRandomQuery(min_time, max_time, time_unit);

    // 执行随机查询并打印结果
    std::cout << "Executing Random Query: " << random_query << std::endl;
    std::cout << client.Query(random_query) << std::endl;

    return 0;
}