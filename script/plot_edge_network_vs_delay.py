import matplotlib.pyplot as plt
import matplotlib.ticker as ticker  # 导入ticker模块

import numpy as np

plt.rcParams.update({
    'font.sans-serif': 'Noto Sans CJK JP',
    'axes.unicode_minus': False,
    'legend.fontsize': 16,       # 图例字体大小
    'axes.titlesize': 16,       # 子图标题大小
    'axes.labelsize': 16,       # 坐标轴标签大小
    'xtick.labelsize': 16,      # X轴刻度大小
    'ytick.labelsize': 16       # Y轴刻度大小
})

# 数据
edge_network = ['5x5', '6x6', '7x7', '8x8', '9x9', '10x10']
# 平均延时（ms）
hcbf_avg_times =       [5.72, 6.12, 6.30, 6.41, 6.89, 6.96]
trindex_avg_times =    [4.91, 4.96, 4.93, 4.97, 5.11, 5.11]  # 示例数据

# P90延时（ms）
hcbf_p90_times =       [11.66, 14.95, 16.07, 16.88, 18.48, 18.75]
trindex_p90_times =    [9.39, 9.49, 9.47, 9.66, 9.74, 9.68]  # 示例数据

# P95延时（ms）
hcbf_p95_times =       [17.46, 19.61, 20.56, 20.68, 22.11, 22.28]  # 示例数据
trindex_p95_times =    [9.60, 9.86, 9.81, 9.98, 9.92, 9.98]  # 示例数据

# P99延时（ms）
hcbf_p99_times =       [23.15, 24.52, 25.16, 25.52, 26.32, 26.46]  # 示例数据
trindex_p99_times =    [13.34, 13.68, 13.60, 13.76, 13.70, 13.76]  # 示例数据

# 创建子图
fig, axs = plt.subplots(2, 2, figsize=(12, 10))

# 创建子图后，统一修改每个子图的代码结构：

# 平均延时图
axs[0, 0].plot(edge_network, hcbf_avg_times, '-o', color='#B19CD9', label='HCBF-Tree')
axs[0, 0].plot(edge_network, trindex_avg_times, '-^', color='#FF6347', label='TRQIndex')
axs[0, 0].set_title('平均数据检索延时', fontsize=16)
axs[0, 0].set_xlabel('边缘网络规模', fontsize=16)
axs[0, 0].set_ylabel('数据检索延时 (ms)', fontsize=16)
axs[0, 0].yaxis.set_major_locator(ticker.MaxNLocator(integer=True))  # 新增这行使纵坐标为整数
#axs[0, 0].set_ylim(5, 35)  # 新增这行设置y轴上限为30
axs[0, 0].legend(loc='upper left', fontsize=16)  # 修改图例位置
axs[0, 0].grid(True)

# P90延时图
axs[0, 1].plot(edge_network, hcbf_p90_times, '-o', color='#B19CD9', label='HCBF-Tree')
axs[0, 1].plot(edge_network, trindex_p90_times, '-^', color='#FF6347', label='TRQIndex')
axs[0, 1].set_title('P90数据检索延时', fontsize=16)
axs[0, 1].set_xlabel('边缘网络规模', fontsize=16)
axs[0, 1].set_ylabel('数据检索延时 (ms)', fontsize=16)
axs[0, 1].set_ylim(5, 35)  # 新增这行设置y轴上限为30
axs[0, 1].legend(loc='upper left', fontsize=16)  # 修改图例位置
axs[0, 1].grid(True)

# P95延时图
axs[1, 0].plot(edge_network, hcbf_p95_times, '-o', color='#B19CD9', label='HCBF-Tree')
axs[1, 0].plot(edge_network, trindex_p95_times, '-^', color='#FF6347', label='TRQIndex')
axs[1, 0].set_title('P95数据检索延时', fontsize=16)
axs[1, 0].set_xlabel('边缘网络规模', fontsize=16)
axs[1, 0].set_ylabel('数据检索延时 (ms)', fontsize=16)
axs[1, 0].set_ylim(5, 35)  # 新增这行设置y轴上限为30
axs[1, 0].legend(loc='upper left', fontsize=16)  # 修改图例位置
axs[1, 0].grid(True)

# P99延时图
axs[1, 1].plot(edge_network, hcbf_p99_times, '-o', color='#B19CD9', label='HCBF-Tree')
axs[1, 1].plot(edge_network, trindex_p99_times, '-^', color='#FF6347', label='TRQIndex')
axs[1, 1].set_title('P99数据检索延时', fontsize=16)
axs[1, 1].set_xlabel('边缘网络规模', fontsize=16)
axs[1, 1].set_ylabel('数据检索延时 (ms)', fontsize=16)
axs[1, 1].yaxis.set_major_locator(ticker.MaxNLocator(integer=True))
axs[1, 1].set_ylim(5, 35)  # 新增这行设置y轴上限为30
axs[1, 1].yaxis.set_major_locator(ticker.MultipleLocator(5))  # 改为每5个单位一个刻度
axs[1, 1].legend(loc='upper left', fontsize=16)  # 修改图例位置
axs[1, 1].grid(True)

# 调整布局并保存图形
plt.tight_layout()
plt.savefig('avg_delay_comparison.png', dpi=500, bbox_inches='tight')
plt.close()