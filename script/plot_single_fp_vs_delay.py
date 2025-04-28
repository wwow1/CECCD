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
single_fp = ['3%', '1%', '0.5%', '0.1%']
# 平均延时（ms）
hcbf_avg_times =       [6.61, 5.14, 5.11, 4.61]
trindex_avg_times =    [4.93, 4.68, 4.62, 4.57]  # 示例数据
allroaring_avg_times = []  # 示例数据

# P90延时（ms）
hcbf_p90_times =       [17.76, 9.94, 9.84, 9.77]
trindex_p90_times =    [9.94, 9.67, 9.66,9.57]  # 示例数据
allroaring_p90_times = []  # 示例数据

# P95延时（ms）
hcbf_p95_times =       [20.47, 16.36, 12.41, 9.81]  # 示例数据
trindex_p95_times =    [12.58, 9.94, 9.82, 9.78]  # 示例数据
allroaring_p95_times = []  # 示例数据

# P99延时（ms）
hcbf_p99_times =       [24.47, 21.84, 21.77, 12.83]  # 示例数据
trindex_p99_times =    [14.88, 13.18, 9.95, 9.92]  # 示例数据
allroaring_p99_times = []  # 示例数据

# 创建子图
fig, axs = plt.subplots(2, 2, figsize=(12, 10))

# 创建子图后，统一修改每个子图的代码结构：

# 平均延时图
axs[0, 0].plot(single_fp, hcbf_avg_times, '-o', color='#B19CD9', label='HCBF-Tree')
axs[0, 0].plot(single_fp, trindex_avg_times, '-^', color='#FF6347', label='TRQIndex')
axs[0, 0].set_title('平均数据检索延时', fontsize=16)
axs[0, 0].set_xlabel('单个计数布隆过滤器的预期假阳性率参数', fontsize=16)
axs[0, 0].set_ylabel('数据检索延时 (ms)', fontsize=16)
axs[0, 0].yaxis.set_major_locator(ticker.MaxNLocator(integer=True))  # 新增这行使纵坐标为整数
axs[0, 0].grid(True)
axs[0, 0].legend(fontsize=16)

# P90延时图
axs[0, 1].plot(single_fp, hcbf_p90_times, '-o', color='#B19CD9', label='HCBF-Tree')
axs[0, 1].plot(single_fp, trindex_p90_times, '-^', color='#FF6347', label='TRQIndex')
axs[0, 1].set_title('P90数据检索延时', fontsize=16)
axs[0, 1].set_xlabel('单个计数布隆过滤器的预期假阳性率参数', fontsize=16)
axs[0, 1].set_ylabel('数据检索延时 (ms)', fontsize=16)
axs[0, 1].yaxis.set_major_locator(ticker.MaxNLocator(integer=True))  # 新增这行使纵坐标为整数
axs[0, 1].set_ylim(5, 30)  # 新增这行设置y轴上限为30
axs[0, 1].grid(True)
axs[0, 1].legend(fontsize=16)

# P95延时图
axs[1, 0].plot(single_fp, hcbf_p95_times, '-o', color='#B19CD9', label='HCBF-Tree')
axs[1, 0].plot(single_fp, trindex_p95_times, '-^', color='#FF6347', label='TRQIndex')
axs[1, 0].set_title('P95数据检索延时', fontsize=16)
axs[1, 0].set_xlabel('单个计数布隆过滤器的预期假阳性率参数', fontsize=16)
axs[1, 0].set_ylabel('数据检索延时 (ms)', fontsize=16)
axs[1, 0].yaxis.set_major_locator(ticker.MaxNLocator(integer=True))  # 新增这行使纵坐标为整数
axs[1, 0].set_ylim(5, 30)  # 新增这行设置y轴上限为30
axs[1, 0].grid(True)
axs[1, 0].legend(fontsize=16)

# P99延时图
axs[1, 1].plot(single_fp, hcbf_p99_times, '-o', color='#B19CD9', label='HCBF-Tree')
axs[1, 1].plot(single_fp, trindex_p99_times, '-^', color='#FF6347', label='TRQIndex')
axs[1, 1].set_title('P99数据检索延时', fontsize=16)
axs[1, 1].set_xlabel('单个计数布隆过滤器的预期假阳性率参数', fontsize=16)
axs[1, 1].set_ylim(5, 30)  # 新增这行设置y轴上限为30
axs[1, 1].yaxis.set_major_locator(ticker.MaxNLocator(integer=True))  # 新增这行使纵坐标为整数
axs[1, 1].set_ylabel('数据检索延时 (ms)', fontsize=16)
axs[1, 1].grid(True)
axs[1, 1].legend(fontsize=16)

# 调整布局并保存图形
plt.tight_layout()
plt.savefig('avg_delay_comparison.png', dpi=500, bbox_inches='tight')
plt.close()