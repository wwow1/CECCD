import matplotlib.pyplot as plt
import matplotlib.ticker as ticker  # 导入ticker模块

import numpy as np

# 数据
network_sizes = ['1.0', '0.8', '0.6', '0.4', '0.2']

# P99延时（ms）
hcbf_p99_times =       [16.40, 20.63, 23.86, 25.22, 26.67]  # 示例数据
trindex_p99_times =    [12.33, 12.92, 12.82, 13.12, 13.42]  # 示例数据
allroaring_p99_times = [10.83, 10.86, 10.88, 10.92, 10.89]  # 示例数据

# 创建子图
fig, axs = plt.subplots(2, 2, figsize=(12, 10))

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