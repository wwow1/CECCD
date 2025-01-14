// test_framework/main.cpp
#include "test_executor.h"
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

void runPerformanceTest() {
    TestExecutor executor;
    TestExecutor::TestConfig config{
        .concurrent_clients = 10,
        .queries_per_client = 100,
        .time_window_seconds = 3600,
        .stream_id = "test_stream"
    };
    
    // 运行测试
    auto result = executor.runTest(config);
    
    // 计算统计信息
    std::sort(result.latencies.begin(), result.latencies.end());
    double p95 = result.latencies[result.latencies.size() * 0.95];
    double p99 = result.latencies[result.latencies.size() * 0.99];
    
    // 输出结果
    json output = {
        {"test_config", {
            {"concurrent_clients", config.concurrent_clients},
            {"queries_per_client", config.queries_per_client},
            {"time_window_seconds", config.time_window_seconds}
        }},
        {"results", {
            {"success_count", result.success_count},
            {"error_count", result.error_count},
            {"total_time", result.total_time},
            {"qps", result.qps},
            {"p95_latency", p95},
            {"p99_latency", p99}
        }}
    };
    
    // 保存结果
    std::ofstream out("test_results.json");
    out << output.dump(4);
}

int main() {
    try {
        runPerformanceTest();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
}