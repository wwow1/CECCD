# -*- coding: utf-8 -*-
import matplotlib.pyplot as plt
import numpy as np
network_sizes = ['5x5', '6x6', '7x7', '8x8', '9x9', '10x10']

#false_positive_rates_trindex = [4.8, 4.13, 3.44, 3.05, 2.13, 1.51, 1.22, 0.94]  # 示例数据
plt.rcParams.update({
    'font.sans-serif': 'Noto Sans CJK JP',
    'axes.unicode_minus': False,
    'legend.fontsize': 16,       # 图例字体大小
    'axes.labelsize': 16,       # 坐标轴标签字体大小
    'xtick.labelsize': 16,      # X轴刻度字体大小
    'ytick.labelsize': 16       # Y轴刻度字体大小
})
false_positive_rates = [10.70, 13.26, 15.71, 16.64, 17.30, 18.13]
false_positive_rates_trindex = [3.55, 3.92, 3.88, 3.94, 4.12, 4.19]  # 示例数据

# 创建图形
plt.figure(figsize=(10, 6))

# 绘制折线图，使用浅紫色三角形标记
plt.plot(network_sizes, false_positive_rates, '-o', color='#B19CD9', linewidth=2, markersize=8, label='HCBF-Tree')  # 使用浅紫色
plt.plot(network_sizes, false_positive_rates_trindex, '-^', color='#FF6347', linewidth=2, markersize=8, label='TRQIndex')  # 使用番茄色的实线和圆形标记

#添加数据标签
for i, rate in enumerate(false_positive_rates):
    plt.annotate('{:.2f}%'.format(rate), 
                xy=(i, rate), 
                xytext=(0, 10),
                textcoords='offset points',
                ha='center',
                va='bottom')

for i, rate in enumerate(false_positive_rates_trindex):
    plt.annotate('{:.2f}%'.format(rate), 
                xy=(i, rate), 
                xytext=(0, 10),
                textcoords='offset points',
                ha='center',
                va='bottom')

# 设置标题和标签时也可以单独指定大小
# 设置标题和标签时统一使用16号字体
plt.xlabel('边缘网络规模', fontsize=16)  # 统一X轴标签大小
plt.ylabel('边缘数据索引的假阳性率 (%)', fontsize=16)  # 统一Y轴标签大小

# 图例设置（删除重复的plt.legend()调用）
plt.legend(fontsize=16)  # 统一图例字体大小

# 设置网格
plt.grid(True, linestyle='--', alpha=0.7)

# 设置y轴范围，留出一些空间显示数据标签
plt.ylim(0, max(false_positive_rates) * 1.2)

# 新增：设置y轴刻度为整数
max_y = int(max(false_positive_rates) * 1.2) + 1
plt.yticks(np.arange(0, max_y, 2))  # 每2个单位显示一个刻度

# 添加图例
plt.legend()

# 优化布局
plt.tight_layout()


plt.savefig('false_positive_rates.png', dpi=500, bbox_inches='tight')

plt.close()

