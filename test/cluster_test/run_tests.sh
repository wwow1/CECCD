#!/bin/bash
# run_tests.sh

# 配置测试参数
CONCURRENT_CLIENTS=(1 5 10 20)
QUERIES_PER_CLIENT=(100 500 1000)
TIME_WINDOWS=(300 1800 3600)

# 创建结果目录
mkdir -p test_results

# 运行不同配置的测试
for clients in "${CONCURRENT_CLIENTS[@]}"; do
    for queries in "${QUERIES_PER_CLIENT[@]}"; do
        for window in "${TIME_WINDOWS[@]}"; do
            echo "Running test with $clients clients, $queries queries, ${window}s window"
            
            # 更新配置文件
            cat > test_config.json << EOF
{
    "concurrent_clients": $clients,
    "queries_per_client": $queries,
    "time_window_seconds": $window
}
EOF
            
            # 运行测试
            ./test_client_main
            
            # 移动结果文件
            mv test_results.json "test_results/result_${clients}_${queries}_${window}.json"
        done
    done
done

# 生成汇总报告
python3 analyze_results.py