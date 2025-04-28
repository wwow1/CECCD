# -*- coding: utf-8 -*-
import matplotlib.pyplot as plt
import numpy as np
plt.rcParams.update({
    'font.sans-serif': 'Noto Sans CJK JP',
    'axes.unicode_minus': False,
    'legend.fontsize': 16,       # 图例字体大小
    'axes.labelsize': 16,       # 坐标轴标签字体大小
    'xtick.labelsize': 16,      # X轴刻度字体大小
    'ytick.labelsize': 16       # Y轴刻度字体大小
})
# 数据
network_sizes = ['1.0', '0.8', '0.6', '0.4', '0.2']
# P99检索时间（ms）
hcbf_p99_times = [21.63, 22.77, 22.75, 23.68, 23.84]
hashmap_p99_times = [9.94, 10.01, 9.95, 9.88, 9.86]

# # 绘制平均时间图
# plt.figure(figsize=(8, 6))
# plt.plot(network_sizes, hcbf_avg_times, '-o', color='#B19CD9', linewidth=2, markersize=8, 
#          label='HCBF', alpha=0.9)
# # plt.plot(network_sizes, hashmap_avg_times, 's-', color='#87CEFA', linewidth=2, markersize=8, 
# #          label='HCBF-without-false-positive', alpha=0.9)

# 绘制P99时间图
plt.figure(figsize=(8, 6))
plt.plot(network_sizes, hcbf_p99_times, '-o', color='#B19CD9', linewidth=2, markersize=8, 
         label='HCBF-Tree', alpha=0.9)
plt.plot(network_sizes, hashmap_p99_times, '-^', color='#87CEFA', linewidth=2, markersize=8, 
         label='HCBF-Tree-no-false-positive', alpha=0.9)

# 添加P99时间数据标签
# for i in range(len(network_sizes)):
#     plt.annotate('{:.2f}'.format(hcbf_p99_times[i]), 
#                 xy=(i, hcbf_p99_times[i]), 
#                 xytext=(0, 10),
#                 textcoords='offset points',
#                 ha='center',
#                 va='bottom')
#     plt.annotate('{:.2f}'.format(hashmap_p99_times[i]), 
#                 xy=(i, hashmap_p99_times[i]), 
#                 xytext=(0, 10),
#                 textcoords='offset points',
#                 ha='center',
#                 va='bottom')

# 设置P99时间图的标题和标签
plt.xlabel('Zipfian分布参数', fontsize=16)
plt.ylabel('P99数据检索延时 (ms)', fontsize=16)
plt.legend(loc='upper left', fontsize=16)
plt.grid(True, linestyle='--', alpha=0.7)
plt.ylim(0, max(hcbf_p99_times) * 1.2)

# 优化布局并保存P99时间图
plt.tight_layout()
plt.savefig('p99_lookup_times.png', dpi=500, bbox_inches='tight')
plt.close() 