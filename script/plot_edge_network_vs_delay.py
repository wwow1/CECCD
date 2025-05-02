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
hcbf_avg_times =       [5.32, 5.66, 6.02, 6.15, 6.23, 6.30]
trindex_avg_times =    [4.57, 4.63, 4.68, 4.75, 4.78, 4.79]  # 示例数据

# P90延时（ms）
hcbf_p90_times =       [10.10, 13.81, 16.42, 16.75, 17.98, 18.36]
trindex_p90_times =    [9.44,  9.48, 9.58, 9.62, 9.63, 9.63]  # 示例数据

# P95延时（ms）
hcbf_p95_times =       [16.92, 18.99, 20.90, 20.96, 21.64, 21.84]  # 示例数据
trindex_p95_times =    [9.78, 9.90, 9.87, 9.95, 9.92, 9.86]  # 示例数据

# P99延时（ms）
hcbf_p99_times =       [22.46, 24.18, 25.49, 25.69, 26.40, 26.39]  # 示例数据
trindex_p99_times =    [13.33, 13.41, 13.30, 13.64, 13.67, 13.66]  # 示例数据

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
axs[0, 0].set_ylim(4, 7)  # 新增这行设置y轴上限为30
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