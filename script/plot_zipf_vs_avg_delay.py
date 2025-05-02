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
hcbf_avg_times =       [7.18, 6.46, 5.21, 4.16, 3.11, 2.16, 1.51, 1.12]
trindex_avg_times =    [6.13, 5.55, 4.47, 3.58, 2.68, 1.88, 1.32, 1.00]  # 示例数据
allroaring_avg_times = []  # 示例数据

# P90延时（ms）
hcbf_p90_times =       [14.32, 13.14, 9.90, 9.82, 8.89, 8.24, 7.47, 5.29]
trindex_p90_times =    [9.82, 9.70, 9.44, 9.50, 8.63, 8.16, 7.43, 5.29]  # 示例数据
allroaring_p90_times = []  # 示例数据

# P95延时（ms）
hcbf_p95_times =       [18.30, 18.17, 16.57, 15.26, 12.33, 9.51, 8.90, 8.22]  # 示例数据
trindex_p95_times =    [9.98, 9.73, 9.72, 9.82, 9.51, 9.00, 8.76, 7.96]  # 示例数据
allroaring_p95_times = []  # 示例数据

# P99延时（ms）
hcbf_p99_times =       [23.61, 23.25, 22.61, 21.78, 20.79, 19.35, 17.63, 15.90]  # 示例数据
trindex_p99_times =    [13.56, 13.13, 12.90, 12.69, 11.93, 11.50, 9.87, 9.80]  # 示例数据
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