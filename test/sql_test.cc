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
                std::cout << "EdgeServer::parseSQLQuery table_name: " << select->fromTable->getName() << std::endl;
            }
            if (select->whereClause != nullptr) {
                parseWhereClause(select->whereClause, start_timestamp, end_timestamp);
                std::cout << "EdgeServer::parseSQLQuery start_time " << start_timestamp << " end_time " << end_timestamp << std::endl;
            }
        }
    } else {
        std::cerr << "SQL query parsing failed." << std::endl;
    }

    return {table_name, start_timestamp, end_timestamp};
}

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
    std::string min_time = argv[3];
    std::string max_time = argv[4];

    // 构建 SQL 查询语句
    std::string query = "SELECT * FROM household_power_consumption WHERE date_time >= " + min_time + " AND date_time <= " + max_time + " LIMIT 5;";
    auto [table_name, start_timestamp, end_timestamp] = parseSQLQuery(query);
    std::cout << "Qirgin Query: " << query
              << " Query for table: " << table_name 
              << ", timestamp range: [" << start_timestamp << ", " << end_timestamp << "]" << std::endl;

    std::string query2 = "SELECT AVG(global_active_power) FROM household_power_consumption WHERE date_time >= " + min_time + " AND date_time <= " + max_time + ";";
    std::string query3 = "SELECT COUNT(*) FROM household_power_consumption WHERE voltage > 230 AND date_time >= " + min_time + " AND date_time <= " + max_time + ";";

    // 执行查询并打印结果
    std::cout << "Executing Query 1: " << query << std::endl;
    std::cout << client.Query(query) << std::endl;

    std::cout << "Executing Query 2: " << query2 << std::endl;
    std::cout << client.Query(query2) << std::endl;

    std::cout << "Executing Query 3: " << query3 << std::endl;
    std::cout << client.Query(query3) << std::endl;

    return 0;
}