// test_client.cpp
#include <iostream>
#include "../include/client.h"

// min 1704038400000
// max 1704044400000
#include <random>
#include <vector>

int edge_server_number = 25;
int durations[4] = {1, 4, 7, 10};
    // 添加 Zipf 分布生成器类
class ZipfDistribution {
    private:
        double alpha;    // Zipf 分布的偏度参数
        double zeta;     // 归一化常数
        uint32_t n;      // 元素数量
        std::mt19937 rng;

        // 计算 Zeta 值用于归一化
        double calculateZeta() {
            double sum = 0.0;
            for (uint32_t i = 1; i <= n; i++) {
                sum += 1.0 / std::pow(i, alpha);
            }
            return sum;
        }

    public:
        ZipfDistribution(uint32_t n, double alpha = 0.8) 
            : n(n), alpha(alpha), rng(std::random_device{}()) {
            zeta = calculateZeta();
        }

        // 生成 Zipf 随机数
        uint32_t sample() {
            double u = std::uniform_real_distribution<>(0.0, 1.0)(rng);
            double sum = 0.0;
            for (uint32_t i = 1; i <= n; i++) {
                sum += 1.0 / (std::pow(i, alpha) * zeta);
                if (sum >= u) {
                    return i;
                }
            }
            return n;
        }

        // 获取特定排名的概率
        double getProbability(uint32_t rank) {
            return 1.0 / (std::pow(rank, alpha) * zeta);
        }
};
    
// 生成随机查询语句的函数
std::string generateRandomQuery(int64_t min_time, int64_t max_time, int64_t time_unit, 
                               uint64_t max_table_id, uint64_t edge_server_id, double zipf_alpha) {
    // Check if parameters are valid
    if (min_time > max_time) {
        std::cerr << "Error: min_time is greater than max_time." << std::endl;
        return "";
    }
    if (time_unit <= 0) {
        std::cerr << "Error: time_unit must be greater than 0." << std::endl;
        return "";
    }

    std::random_device rd;
    if (!rd.entropy()) {
        std::cerr << "Warning: std::random_device has no entropy. Using fixed seed." << std::endl;
    }
    std::mt19937 gen(rd.entropy() ? rd() : 42);
    
    // 计算本分区的表格范围
    uint64_t tables_per_server = max_table_id / edge_server_number;
    uint64_t local_min = edge_server_id * tables_per_server;
    uint64_t local_max = (edge_server_id + 1) * tables_per_server - 1;
    
    int table_id = 0;
    // 10%概率查询本分区，90%概率查询其他分区
    std::bernoulli_distribution local_choice(0.10); // 修改概率为10%
    if (local_choice(gen)) {
        // 从本分区随机选择
        std::uniform_int_distribution<uint64_t> local_dist(local_min, local_max);
        table_id = local_dist(gen);
    } else {
        // 从其他分区随机选择
        std::uniform_int_distribution<uint64_t> dis_server(0, edge_server_number - 1);
        uint64_t other_server = dis_server(gen);
        while (other_server == edge_server_id) {
            other_server = dis_server(gen);
        }
        uint64_t other_min = other_server * tables_per_server;
        uint64_t other_max = (other_server + 1) * tables_per_server - 1;
        std::uniform_int_distribution<uint64_t> dis_table(other_min, other_max);
        table_id = dis_table(gen);
    }
    std::uniform_int_distribution<> dis_field(0, 19);  // 随机选择字段
    std::uniform_int_distribution<> dis_num_fields(1, 2);  // 随机选择 1 或 2 个字段
    std::uniform_int_distribution<> dis_aggregation(0, 9);  // 概率性选择聚合语句，10% 使用聚合函数
    // 使用 Zipf 分布选择时间范围
    ZipfDistribution time_zipf((max_time - min_time) / time_unit, zipf_alpha);  // 使用传入的zipf_alpha参数
    
    // 从固定持续时间中随机选择
    std::uniform_int_distribution<> dis_duration(0, 3);
    int duration_index = dis_duration(gen);
    int64_t duration = durations[duration_index] * time_unit;
    
    int64_t time_point_index = time_zipf.sample();
    while (time_point_index < durations[duration_index]) {
        time_point_index = time_zipf.sample();
    }
    // 计算起始时间
    int64_t start_time = max_time - time_point_index * time_unit;
    int64_t end_time = start_time + duration;
    assert(start_time >= min_time);
    assert(end_time <= max_time);

    std::uniform_int_distribution<> dis_filter(0, 3); // 随机选择过滤函数
    std::uniform_int_distribution<uint64_t> dis_table(0, max_table_id); // 随机选择表 ID

    std::vector<std::string> fields = {
        "s_0", "s_1", "s_2", "s_3", "s_4", "s_5",
        "s_6", "s_7", "s_8", "s_9", "s_10", "s_11", "s_12", "s_13", "s_14",
        "s_15", "s_16", "s_17", "s_18", "s_19"
    };

    std::string query = "SELECT ";
    std::vector<std::string> non_aggregate_fields; // 存储非聚合字段
    std::vector<std::string> aggregate_expressions; // 存储聚合表达式

    // 随机选择一个或两个字段
    int num_fields = dis_num_fields(gen);
    std::vector<std::string> filter_conditions; // 用于存储 WHERE 子句的过滤条件
    for (int i = 0; i < num_fields; ++i) {
        if (i > 0) query += ", ";
        int field_index = dis_field(gen);
        std::string field = fields[field_index];

        // 概率性地追加聚合语句，10% 使用聚合函数
        int aggregation = dis_aggregation(gen);
        if (aggregation >= 1 && aggregation <= 5) {
            std::string agg_expr;
            switch (aggregation) {
                case 1:
                    agg_expr = "MAX(" + field + ")";
                    break;
                case 2:
                    agg_expr = "MIN(" + field + ")";
                    break;
                case 3:
                    agg_expr = "AVG(" + field + ")";
                    break;
                case 4:
                    agg_expr = "SUM(" + field + ")";
                    break;
                case 5:
                    agg_expr = "COUNT(" + field + ")";
                    break;
            }
            aggregate_expressions.push_back(agg_expr);
            query += agg_expr;
        } else {
            non_aggregate_fields.push_back(field);
            query += field;
        }

        // 追加随机过滤函数到过滤条件列表
        int filter = dis_filter(gen);
        switch (filter) {
            case 0:
                filter_conditions.push_back(field + " > 0");
                break;
            case 1:
                filter_conditions.push_back(field + " < 100");
                break;
            case 2:
                filter_conditions.push_back(field + " IS NOT NULL");
                break;
            case 3:
                filter_conditions.push_back(field + " = 50");
                break;
        }
    }

    // 随机选择表名
    std::string table_name = "d_" + std::to_string(table_id);
    query += " FROM " + table_name;

    // 构建 WHERE 子句
    query += " WHERE date_time >= " + std::to_string(start_time) + " AND date_time <= " + std::to_string(end_time);
    if (!filter_conditions.empty()) {
        for (const auto& condition : filter_conditions) {
            query += " AND " + condition;
        }
    }

    // 如果有非聚合字段和聚合表达式，添加 GROUP BY 子句
    if (!non_aggregate_fields.empty() && !aggregate_expressions.empty()) {
        query += " GROUP BY ";
        for (size_t i = 0; i < non_aggregate_fields.size(); ++i) {
            if (i > 0) query += ", ";
            query += non_aggregate_fields[i];
        }
    }

    return query;
}

// 修改 main 函数以使用生成的随机查询
int main(int argc, char* argv[]) {
    // 检查参数数量
    if (argc != 10) { // 修改为10个参数
        std::cerr << "Usage: " << argv[0] << " <server_address> <port> <min_time> <max_time> <time_unit> <max_table_id> <edge_server_id> <edge_server_number> <zipf_alpha>" << std::endl;
        return 1;
    }

    // 创建 Client 实例，连接到服务器
    std::string server_address = std::string(argv[1]) + ":" + argv[2];
    std::cout << " server_address " << server_address << std::endl;
    Client client(server_address);

    // 获取参数 - 按照新的参数顺序解析
    int64_t min_time = std::stoll(argv[3]);
    int64_t max_time = std::stoll(argv[4]);
    int64_t time_unit = std::stoll(argv[5]);      // 第5个参数是time_unit
    uint64_t max_table_id = std::stoull(argv[6]); // 第6个参数是max_table_id
    uint64_t edge_server_id = std::stoull(argv[7]); // 第7个参数是edge_server_id
    edge_server_number = std::stoi(argv[8]);      // 第8个参数是edge_server_number
    double zipf_alpha = std::stod(argv[9]);       // 第9个参数是zipf_alpha

    // 生成随机查询语句
    std::string random_query = generateRandomQuery(min_time, max_time, time_unit, max_table_id, edge_server_id, zipf_alpha);

    // 执行随机查询并打印结果
    std::cout << "Executing Random Query: " << random_query << std::endl;
    std::string result = client.Query(random_query); // 检查 Query 方法是否有问题
    // // 简单示例：逐行处理结果
    // std::istringstream iss(result);
    // std::string line;
    // while (std::getline(iss, line)) {
    //     // 这里可以添加更复杂的解析逻辑
    //     std::cout << "Parsed line: " << line << std::endl;
    // }
    return 0;
}