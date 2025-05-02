// test_client.cpp
#include <iostream>
#include "../include/client.h"
#include <random>
#include <vector>
#include <thread>  // 添加这个头文件

int edge_server_number = 0;
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
    std::bernoulli_distribution local_choice(0.90); // 修改概率为10%
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
    if (argc != 9) {
        std::cerr << "Usage: " << argv[0] << " <center_server_ip> <center_server_port> <min_time> <max_time> <time_unit> <zipf_alpha> <period_seconds> <max_table_id>" << std::endl;
        return 1;
    }

    // Create Client instance - connects only to center server
    std::string center_server_address = std::string(argv[1]) + ":" + argv[2];
    Client client(center_server_address);

    // Get parameters
    int64_t min_time = std::stoll(argv[3]);
    int64_t max_time = std::stoll(argv[4]);
    int64_t time_unit = std::stoll(argv[5]);
    double zipf_alpha = std::stod(argv[6]);
    int period_seconds = std::stoi(argv[7]);
    int max_table_id = std::stoi(argv[7]);

    // 1. 获取所有edge节点信息
    auto edge_nodes = client.GetEdgeNodes();
    if (edge_nodes.empty()) {
        std::cerr << "Error: No edge nodes available" << std::endl;
        return 1;
    }
    edge_server_number = edge_nodes.size(); // 动态设置edge_server_number

    std::cout << "Found " << edge_nodes.size() << " edge nodes" << std::endl;

    // 2. Warm-up period (30 seconds)
    const int warmup_duration = period_seconds * 2;
    std::cout << "\n=== Starting warm-up period (" << warmup_duration << " seconds) ===" << std::endl;
    auto warmup_start = std::chrono::steady_clock::now();
    while (std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - warmup_start).count() < warmup_duration) {
        
        std::vector<std::thread> warmup_threads;
        for (size_t i = 0; i < edge_nodes.size(); ++i) {
            warmup_threads.emplace_back([&, i]() {
                try {
                    Client node_client(center_server_address);
                    node_client.BindToEdgeServer(edge_nodes[i]);
                    std::string query = generateRandomQuery(
                        min_time, max_time, time_unit,
                        max_table_id,
                        i,
                        zipf_alpha
                    );
                    node_client.Query(query);
                } catch (...) {
                    // Ignore warm-up errors
                }
            });
        }
        for (auto& t : warmup_threads) {
            t.join();
        }
    }
    std::cout << "=== Warm-up period completed ===" << std::endl;
    // 3. Waiting period
    std::cout << "\n=== Starting waiting period (" << period_seconds << " seconds) ===" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(period_seconds));
    std::cout << "=== Waiting period completed ===" << std::endl;

    // 4. Execution period
    const int execution_queries = 1000;
    std::cout << "\n=== Starting execution period (" << execution_queries << " queries per node) ===" << std::endl;
    
    std::atomic<int64_t> total_latency_ms{0};
    std::vector<std::thread> execution_threads;
    
    for (size_t node_idx = 0; node_idx < edge_nodes.size(); ++node_idx) {
        execution_threads.emplace_back([&, node_idx]() {
            try {
                Client node_client(center_server_address);
                node_client.BindToEdgeServer(edge_nodes[node_idx]);
                for (int i = 0; i < execution_queries; ++i) {
                    auto start = std::chrono::steady_clock::now();
                    
                    std::string query = generateRandomQuery(
                        min_time, max_time, time_unit,
                        max_table_id,
                        node_idx,
                        zipf_alpha
                    );
                    node_client.Query(query);
                    
                    auto end = std::chrono::steady_clock::now();
                    total_latency_ms += std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
                }
            } catch (const std::exception& e) {
                std::cerr << "Execution error: " << e.what() << std::endl;
            }
        });
    }
    
    int64_t current = total_latency_ms.load();
    while (!total_latency_ms.compare_exchange_weak(current, current * 2)) {
    }

    for (auto& t : execution_threads) {
        t.join();
    }

    // Output results
    std::cout << "\n=== Test completed ===" << std::endl;
    std::cout << "Total requests: " << edge_nodes.size() * execution_queries << std::endl;
    std::cout << "Total latency (ms): " << total_latency_ms << std::endl;
    std::cout << "Average latency (ms): " << static_cast<double>(total_latency_ms) / (edge_nodes.size() * execution_queries) << std::endl;

    return 0;
}