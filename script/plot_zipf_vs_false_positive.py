# -*- coding: utf-8 -*-
import matplotlib.pyplot as plt
import numpy as np
# motivation
zipf = ['1.0', '0.8', '0.6', '0.4', '0.2']

#false_positive_rates_trindex = [4.8, 4.13, 3.44, 3.05, 2.13, 1.51, 1.22, 0.94]  # 示例数据
plt.rcParams.update({
    'font.sans-serif': 'Noto Sans CJK JP',
    'axes.unicode_minus': False,
    'legend.fontsize': 16,       # 图例字体大小
    'axes.labelsize': 16,       # 坐标轴标签字体大小
    'xtick.labelsize': 16,      # X轴刻度字体大小
    'ytick.labelsize': 16       # Y轴刻度字体大小
})
# 数据
false_positive_rates = [6.24, 8.47, 10.46, 12.81, 14.41]

#zipf = ['0.2', '0.4', '0.6', '0.8', '1.0', '1.2', '1.4', '1.6']
#false_positive_rates = [14.05, 12.34, 10.53, 7.96, 5.88, 4.33, 3.01, 2.14]


false_positive_rates_trindex = [4.8, 4.13, 3.44, 3.05, 2.13, 1.51, 1.22, 0.94]  # 示例数据

# 创建图形
plt.figure(figsize=(10, 6))

# 绘制折线图，使用浅紫色三角形标记
plt.plot(zipf, false_positive_rates, '-o', color='#B19CD9', linewidth=2, markersize=8, label='HCBF-Tree')  # 使用浅紫色
#plt.plot(zipf, false_positive_rates_trindex, '-^', color='#FF6347', linewidth=2, markersize=8, label='TRQIndex')  # 使用番茄色的实线和圆形标记

#添加数据标签
for i, rate in enumerate(false_positive_rates):
    plt.annotate('{:.2f}%'.format(rate), 
                xy=(i, rate), 
                xytext=(0, 10),
                textcoords='offset points',
                ha='center',
                va='bottom')

# for i, rate in enumerate(false_positive_rates_trindex):
#     plt.annotate('{:.2f}%'.format(rate), 
#                 xy=(i, rate), 
#                 xytext=(0, 10),
#                 textcoords='offset points',
#                 ha='center',
#                 va='bottom')

# 设置标题和标签时也可以单独指定大小
# 设置标题和标签时统一使用16号字体
plt.xlabel('Zipfian分布参数', fontsize=16)  # 统一X轴标签大小
plt.ylabel('边缘数据索引的假阳性率 (%)', fontsize=16)  # 统一Y轴标签大小

# 图例设置（删除重复的plt.legend()调用）
plt.legend(fontsize=16)  # 统一图例字体大小

# 设置网格
plt.grid(True, linestyle='--', alpha=0.7)

# 设置y轴范围，留出一些空间显示数据标签
plt.ylim(0, max(false_positive_rates) * 1.2)

# 添加图例
plt.legend()

# 优化布局
plt.tight_layout()


plt.savefig('false_positive_rates.png', dpi=500, bbox_inches='tight')

plt.close()

