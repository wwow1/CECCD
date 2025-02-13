import matplotlib.pyplot as plt
import matplotlib.ticker as ticker  # 导入ticker模块

import numpy as np

# 数据
network_sizes = ['0.4', '0.6', '0.8', '1.0', '1.2', '1.4', '1.6']
# 平均延时（ms）
hcbf_avg_times =       [8.22, 7.28, 6.59, 5.58, 4.91, 4.43, 4.17]
trindex_avg_times =    [7.01, 6.23, 5.50, 4.64, 4.02, 3.58, 3.27]  # 示例数据
allroaring_avg_times = [6.93, 6.15, 5.35, 4.56, 3.91, 3.49, 3.18]  # 示例数据

# P90延时（ms）
hcbf_p90_times =       [16.18, 15.56, 15.08, 13.73, 12.92, 12.38, 12.38]
trindex_p90_times =    [10.71, 10.54, 10.57, 10.04, 9.97, 9.63, 9.73]  # 示例数据
allroaring_p90_times = [10.59, 10.47, 10.22, 10.10, 9.43, 9.57, 9.27]  # 示例数据

# P95延时（ms）
hcbf_p95_times =       [20.34, 20.04, 19.55, 18.67, 18.62, 18.58, 18.44]  # 示例数据
trindex_p95_times =    [10.93, 10.85, 10.88, 10.69, 10.66, 10.43, 10.60]  # 示例数据
allroaring_p95_times = [10.80, 10.77, 10.61, 10.55, 10.26, 10.37, 10.14]  # 示例数据

# P99延时（ms）
hcbf_p99_times =       [24.92, 24.66, 23.91, 23.68, 23.74, 23.41, 23.20]  # 示例数据
trindex_p99_times =    [13.61, 12.86, 13.43, 12.88, 13.17, 13.14, 12.68]  # 示例数据
allroaring_p99_times = [10.96, 10.95, 10.92, 10.91, 10.85, 10.88, 10.83]  # 示例数据

# 创建子图
fig, axs = plt.subplots(2, 2, figsize=(12, 10))

# 绘制平均延时图
axs[0, 0].plot(network_sizes, hcbf_avg_times, '-o', color='#B19CD9', label='HCBF')
axs[0, 0].plot(network_sizes, trindex_avg_times, '-o', color='#FF6347', label='TRQIndex')
#axs[0, 0].plot(network_sizes, allroaring_avg_times, '-o', color='#87CEFA', label='AllRoaring')
axs[0, 0].set_title('Average Delay')
axs[0, 0].set_xlabel('The Parameters of Zipf Distribution')
axs[0, 0].set_ylabel('Discovery Delay (ms)')
axs[0, 0].legend()
axs[0, 0].grid(True)

# 绘制P90延时图
axs[0, 1].plot(network_sizes, hcbf_p90_times, '-o', color='#B19CD9', label='HCBF')
axs[0, 1].plot(network_sizes, trindex_p90_times, '-o', color='#FF6347', label='TRQIndex')
#axs[0, 1].plot(network_sizes, allroaring_p90_times, '-o', color='#87CEFA', label='AllRoaring')
axs[0, 1].set_title('P90 Delay')
axs[0, 1].set_xlabel('The Parameters of Zipf Distribution')
axs[0, 1].set_ylabel('Discovery Delay (ms)')
axs[0, 1].legend()
axs[0, 1].grid(True)

# 绘制P95延时图
axs[1, 0].plot(network_sizes, hcbf_p95_times, '-o', color='#B19CD9', label='HCBF')
axs[1, 0].plot(network_sizes, trindex_p95_times, '-o', color='#FF6347', label='TRQIndex')
#axs[1, 0].plot(network_sizes, allroaring_p95_times, '-o', color='#87CEFA', label='AllRoaring')
axs[1, 0].set_title('P95 Delay')
axs[1, 0].set_xlabel('The Parameters of Zipf Distribution')
axs[1, 0].set_ylabel('Discovery Delay (ms)')
axs[1, 0].legend()
axs[1, 0].grid(True)

# 绘制P99延时图
axs[1, 1].plot(network_sizes, hcbf_p99_times, '-o', color='#B19CD9', label='HCBF')
axs[1, 1].plot(network_sizes, trindex_p99_times, '-o', color='#FF6347', label='TRQIndex')
#axs[1, 1].plot(network_sizes, allroaring_p99_times, '-o', color='#87CEFA', label='AllRoaring')
axs[1, 1].set_title('P99 Delay')
axs[1, 1].set_xlabel('The Parameters of Zipf Distribution')
axs[1, 1].set_ylabel('Discovery Delay (ms)')
axs[1, 1].set_ylim(10, 30)  # 设置y轴范围
axs[1, 1].yaxis.set_major_locator(ticker.MaxNLocator(integer=True))  # 设置y轴刻度为整数
axs[1, 1].legend(loc='upper right')
axs[1, 1].grid(True)

# 调整布局并保存图形
plt.tight_layout()
plt.savefig('avg_delay_comparison.png', dpi=300, bbox_inches='tight')
plt.close() 