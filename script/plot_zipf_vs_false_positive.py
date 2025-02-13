# -*- coding: utf-8 -*-
import matplotlib.pyplot as plt
import numpy as np

# 数据
network_sizes = ['0.4', '0.6', '0.8', '1.0', '1.2', '1.4', '1.6']
false_positive_rates = [17.74, 16.86, 15.80, 13.97, 12.64, 12.60, 11.96]

# 假阳性率数据（请替换为您的数据）
false_positive_rates_trindex = [4.64, 4.44, 4.03, 3.85, 3.34, 3.06, 2.88]  # 示例数据

# 设置中文字体（如果需要显示中文）
plt.rcParams['font.sans-serif'] = ['SimHei']  # 用来正常显示中文标签
plt.rcParams['axes.unicode_minus'] = False    # 用来正常显示负号

# 创建图形
plt.figure(figsize=(10, 6))

# 绘制折线图，使用浅紫色三角形标记
plt.plot(network_sizes, false_positive_rates, '^-', color='#B19CD9', linewidth=2, markersize=8, label='HCBF')  # 使用浅紫色
plt.plot(network_sizes, false_positive_rates_trindex, '-o', color='#FF6347', linewidth=2, markersize=8, label='TRQIndex')  # 使用番茄色的实线和圆形标记

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

# 设置标题和标签
plt.xlabel('The Parameters of Zipf Distribution', fontsize=12)
plt.ylabel('False Positive Rate (%)', fontsize=12)

# 设置网格
plt.grid(True, linestyle='--', alpha=0.7)

# 设置y轴范围，留出一些空间显示数据标签
plt.ylim(0, max(false_positive_rates) * 1.2)

# 添加图例
plt.legend()

# 优化布局
plt.tight_layout()

# 保存图片
plt.savefig('false_positive_rates.png', dpi=300, bbox_inches='tight')
plt.close()

