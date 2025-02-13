import matplotlib.pyplot as plt
import matplotlib.ticker as ticker  # 导入ticker模块

import numpy as np

# 数据
network_sizes = ['3x3', '4x4', '5x5', '6x6', '7x7']
# 平均延时（ms）
hcbf_avg_times =       [4.18, 4.81, 5.60, 6.21, 6.99]
trindex_avg_times =    [4.00, 4.33, 4.63, 4.90, 5.10]  # 示例数据
allroaring_avg_times = [4.02, 4.26, 4.52, 4.86, 5.05]  # 示例数据

# P90延时（ms）
hcbf_p90_times =       [9.79, 10.72, 14.10, 16.51, 18.73]
trindex_p90_times =    [9.51, 9.98, 10.10, 10.05, 10.53]  # 示例数据
allroaring_p90_times = [9.67, 9.77, 9.82, 10.24, 9.96]  # 示例数据

# P95延时（ms）
hcbf_p95_times =       [10.81, 14.94, 18.89, 20.59, 22.48]  # 示例数据
trindex_p95_times =    [10.24, 10.69, 10.70, 10.67, 10.87]  # 示例数据
allroaring_p95_times = [10.18, 10.30, 10.40, 10.62, 10.47]  # 示例数据

# P99延时（ms）
hcbf_p99_times =       [16.40, 20.63, 23.86, 25.22, 26.67]  # 示例数据
trindex_p99_times =    [12.33, 12.92, 12.82, 13.12, 13.42]  # 示例数据
allroaring_p99_times = [10.83, 10.86, 10.88, 10.92, 10.89]  # 示例数据

# 创建子图
fig, axs = plt.subplots(2, 2, figsize=(12, 10))

# 绘制平均延时图
axs[0, 0].plot(network_sizes, hcbf_avg_times, '-o', color='#B19CD9', label='HCBF')
axs[0, 0].plot(network_sizes, trindex_avg_times, '-o', color='#FF6347', label='TRQIndex')
#axs[0, 0].plot(network_sizes, allroaring_avg_times, '-o', color='#87CEFA', label='AllRoaring')
axs[0, 0].set_title('Average Delay')
axs[0, 0].set_xlabel('Edge Network Size')
axs[0, 0].set_ylabel('Discovery Delay (ms)')
axs[0, 0].legend()
axs[0, 0].grid(True)

# 绘制P90延时图
axs[0, 1].plot(network_sizes, hcbf_p90_times, '-o', color='#B19CD9', label='HCBF')
axs[0, 1].plot(network_sizes, trindex_p90_times, '-o', color='#FF6347', label='TRQIndex')
#axs[0, 1].plot(network_sizes, allroaring_p90_times, '-o', color='#87CEFA', label='AllRoaring')
axs[0, 1].set_title('P90 Delay')
axs[0, 1].set_xlabel('Edge Network Size')
axs[0, 1].set_ylabel('Discovery Delay (ms)')
axs[0, 1].legend()
axs[0, 1].grid(True)

# 绘制P95延时图
axs[1, 0].plot(network_sizes, hcbf_p95_times, '-o', color='#B19CD9', label='HCBF')
axs[1, 0].plot(network_sizes, trindex_p95_times, '-o', color='#FF6347', label='TRQIndex')
#axs[1, 0].plot(network_sizes, allroaring_p95_times, '-o', color='#87CEFA', label='AllRoaring')
axs[1, 0].set_title('P95 Delay')
axs[1, 0].set_xlabel('Edge Network Size')
axs[1, 0].set_ylabel('Discovery Delay (ms)')
axs[1, 0].legend()
axs[1, 0].grid(True)

# 绘制P99延时图
axs[1, 1].plot(network_sizes, hcbf_p99_times, '-o', color='#B19CD9', label='HCBF')
axs[1, 1].plot(network_sizes, trindex_p99_times, '-o', color='#FF6347', label='TRQIndex')
#axs[1, 1].plot(network_sizes, allroaring_p99_times, '-o', color='#87CEFA', label='AllRoaring')
axs[1, 1].set_title('P99 Delay')
axs[1, 1].set_xlabel('Edge Network Size')
axs[1, 1].set_ylabel('Discovery Delay (ms)')
axs[1, 1].legend()
axs[1, 1].grid(True)

# 调整布局并保存图形
plt.tight_layout()
plt.savefig('avg_delay_comparison.png', dpi=300, bbox_inches='tight')
plt.close() 