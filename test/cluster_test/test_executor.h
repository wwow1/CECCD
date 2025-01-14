// test_framework/test_executor.h
#ifndef TEST_EXECUTOR_H
#define TEST_EXECUTOR_H

#include "test_client.h"
#include "workload_generator.h"
#include <thread>
#include <future>
#include <queue>
#include <mutex>
#include <condition_variable>

class TestExecutor {
public:
    struct TestConfig {
        int concurrent_clients;
        int queries_per_client;
        int time_window_seconds;
        std::string stream_id;
    };
    
    struct TestResult {
        std::vector<double> latencies;
        int success_count;
        int error_count;
        double total_time;
        double qps;
    };
    
    TestResult runTest(const TestConfig& config) {
        TestResult result;
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // 创建线程池
        std::vector<std::thread> threads;
        std::vector<std::future<std::vector<QueryResult>>> futures;
        
        // 启动测试线程
        for (int i = 0; i < config.concurrent_clients; i++) {
            std::packaged_task<std::vector<QueryResult>(TestConfig)> task(
                [this](TestConfig cfg) {
                    return runClientWorkload(cfg);
                }
            );
            
            futures.push_back(task.get_future());
            threads.emplace_back(std::move(task), config);
        }
        
        // 收集结果
        for (auto& future : futures) {
            auto client_results = future.get();
            for (const auto& qr : client_results) {
                if (qr.success) {
                    result.success_count++;
                    result.latencies.push_back(qr.latency_ms);
                } else {
                    result.error_count++;
                }
            }
        }
        
        // 等待所有线程完成
        for (auto& thread : threads) {
            thread.join();
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        result.total_time = std::chrono::duration_cast<std::chrono::seconds>
            (end_time - start_time).count();
        
        result.qps = (result.success_count + result.error_count) / result.total_time;
        
        return result;
    }

private:
    std::vector<QueryResult> runClientWorkload(const TestConfig& config) {
        TestClient client("localhost:50051");  // 使用实际的服务器地址
        std::vector<QueryResult> results;
        
        auto queries = WorkloadGenerator::generateBatchQueries(
            config.queries_per_client,
            config.stream_id,
            config.time_window_seconds
        );
        
        for (const auto& query : queries) {
            results.push_back(client.executeQuery(query));
        }
        
        return results;
    }
};

#endif // TEST_EXECUTOR_H