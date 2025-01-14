# analyze_results.py
import json
import glob
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns

def analyze_results():
    # 读取所有测试结果
    results = []
    for file in glob.glob("test_results/*.json"):
        with open(file) as f:
            data = json.load(f)
            results.append({
                "concurrent_clients": data["test_config"]["concurrent_clients"],
                "queries_per_client": data["test_config"]["queries_per_client"],
                "time_window": data["test_config"]["time_window_seconds"],
                "qps": data["results"]["qps"],
                "p95_latency": data["results"]["p95_latency"],
                "p99_latency": data["results"]["p99_latency"],
                "success_rate": data["results"]["success_count"] / 
                    (data["results"]["success_count"] + data["results"]["error_count"])
            })
    
    df = pd.DataFrame(results)
    
    # 绘制性能图表
    plt.figure(figsize=(15, 10))
    
    # QPS vs Concurrent Clients
    plt.subplot(2, 2, 1)
    sns.boxplot(data=df, x="concurrent_clients", y="qps")
    plt.title("QPS vs Concurrent Clients")
    
    # Latency vs Concurrent Clients
    plt.subplot(2, 2, 2)
    sns.boxplot(data=df, x="concurrent_clients", y="p95_latency")
    plt.title("P95 Latency vs Concurrent Clients")
    
    # Success Rate vs Load
    plt.subplot(2, 2, 3)
    sns.scatterplot(data=df, x="qps", y="success_rate")
    plt.title("Success Rate vs QPS")
    
    plt.tight_layout()
    plt.savefig("test_results/performance_analysis.png")
    
    # 生成报告
    with open("test_results/summary_report.txt", "w") as f:
        f.write("Performance Test Summary\n")
        f.write("======================\n\n")
        f.write(f"Total test cases: {len(df)}\n")
        f.write(f"Average QPS: {df['qps'].mean():.2f}\n")
        f.write(f"Average P95 latency: {df['p95_latency'].mean():.2f}ms\n")
        f.write(f"Average success rate: {df['success_rate'].mean()*100:.2f}%\n")

if __name__ == "__main__":
    analyze_results()