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
network_sizes = ['0.2', '0.4', '0.6', '0.8', '1.0', '1.2', '1.4', '1.6']
# 平均延时（ms）
hcbf_avg_times =       [8.10, 7.31, 6.55, 5.23, 4.37, 3.54, 2.86, 2.48]
trindex_avg_times =    [7.25, 6.56, 5.89, 4.77, 4.01, 3.28, 2.71, 2.37]  # 示例数据
allroaring_avg_times = []  # 示例数据

# P90延时（ms）
hcbf_p90_times =       [13.60, 12.15, 10.98, 10.63, 10.42, 9.86, 8.51, 5.92]
trindex_p90_times =    [10.71, 10.44, 10.69, 10.21, 10.09, 9.62, 8.44, 5.92]  # 示例数据
allroaring_p90_times = []  # 示例数据

# P95延时（ms）
hcbf_p95_times =       [18.14, 17.46, 16.86, 14.18, 12.14, 10.79, 10.02, 9.10]  # 示例数据
trindex_p95_times =    [10.92, 10.90, 10.85, 10.69, 10.62, 10.40, 9.67, 8.95]  # 示例数据
allroaring_p95_times = []  # 示例数据

# P99延时（ms）
hcbf_p99_times =       [22.84, 22.78, 21.96, 21.21, 20.81, 19.86, 17.58, 15.54]  # 示例数据
trindex_p99_times =    [13.36, 13.27, 12.87, 12.53, 12.26, 11.13, 10.93, 10.73]  # 示例数据
allroaring_p99_times = []  # 示例数据

# 创建子图
fig, axs = plt.subplots(2, 2, figsize=(12, 10))

# 绘制平均延时图
axs[0, 0].plot(network_sizes, hcbf_avg_times, '-o', color='#B19CD9', label='HCBF-Tree')
axs[0, 0].plot(network_sizes, trindex_avg_times, '-^', color='#FF6347', label='TRQIndex')
#axs[0, 0].plot(network_sizes, allroaring_avg_times, '-o', color='#87CEFA', label='AllRoaring')
axs[0, 0].set_title('平均数据检索延时', fontsize=16)
axs[0, 0].set_xlabel('Zipfian分布参数', fontsize=16)
axs[0, 0].set_ylabel('数据检索延时 (ms)', fontsize=16)
axs[0, 0].yaxis.set_major_locator(ticker.MaxNLocator(integer=True))  # 设置y轴刻度为整数
axs[0, 0].legend(fontsize=16)
axs[0, 0].grid(True)

# 绘制P90延时图
axs[0, 1].plot(network_sizes, hcbf_p90_times, '-o', color='#B19CD9', label='HCBF-Tree')
axs[0, 1].plot(network_sizes, trindex_p90_times, '-^', color='#FF6347', label='TRQIndex')
#axs[0, 1].plot(network_sizes, allroaring_p90_times, '-o', color='#87CEFA', label='AllRoaring')
axs[0, 1].set_title('P90数据检索延时', fontsize=16)
axs[0, 1].set_xlabel('Zipfian分布参数', fontsize=16)
axs[0, 1].set_ylabel('数据检索延时 (ms)', fontsize=16)
axs[0, 1].set_ylim(5, 25)  # 新增这行设置y轴上限为30
axs[0, 1].yaxis.set_major_locator(ticker.MaxNLocator(integer=True))  # 设置y轴刻度为整数

axs[0, 1].legend(fontsize=16)
axs[0, 1].grid(True)

# 绘制P95延时图
axs[1, 0].plot(network_sizes, hcbf_p95_times, '-o', color='#B19CD9', label='HCBF-Tree')
axs[1, 0].plot(network_sizes, trindex_p95_times, '-^', color='#FF6347', label='TRQIndex')
#axs[1, 0].plot(network_sizes, allroaring_p95_times, '-o', color='#87CEFA', label='AllRoaring')
axs[1, 0].set_title('P95数据检索延时', fontsize=16)
axs[1, 0].set_xlabel('Zipfian分布参数', fontsize=16)
axs[1, 0].set_ylabel('数据检索延时 (ms)', fontsize=16)
axs[1, 0].set_ylim(5, 25)  # 新增这行设置y轴上限为30
axs[1, 0].yaxis.set_major_locator(ticker.MaxNLocator(integer=True))  # 设置y轴刻度为整数

axs[1, 0].legend(fontsize=16)
axs[1, 0].grid(True)

# 绘制P99延时图
axs[1, 1].plot(network_sizes, hcbf_p99_times, '-o', color='#B19CD9', label='HCBF-Tree')
axs[1, 1].plot(network_sizes, trindex_p99_times, '-^', color='#FF6347', label='TRQIndex')
#axs[1, 1].plot(network_sizes, allroaring_p99_times, '-o', color='#87CEFA', label='AllRoaring')
axs[1, 1].set_title('P99数据检索延时', fontsize=16)
axs[1, 1].set_xlabel('Zipfian分布参数', fontsize=16)
axs[1, 1].set_ylabel('数据检索延时 (ms)', fontsize=16)
axs[1, 1].set_ylim(5, 25)  # 设置y轴范围
axs[1, 1].yaxis.set_major_locator(ticker.MaxNLocator(integer=True))  # 设置y轴刻度为整数
axs[1, 1].legend(loc='upper right')
axs[1, 1].grid(True)

# 调整布局并保存图形
plt.tight_layout()
plt.savefig('avg_delay_comparison.png', dpi=500, bbox_inches='tight')
plt.close()