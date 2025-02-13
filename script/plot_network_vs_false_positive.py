# -*- coding: utf-8 -*-
import matplotlib.pyplot as plt
import numpy as np

# 数据

network_sizes = ['3x3', '4x4', '5x5', '6x6', '7x7']
false_positive_rates = [5.44, 9.12, 14.26, 19.83, 25.32]
# motivation数据
#false_positive_rates = [5.59, 9.38, 14.60, 19.59, 25.50]
# 假阳性率数据（请替换为您的数据）
false_positive_rates_trindex = [2.87, 3.40, 3.46, 3.82, 3.78]  # 示例数据

# 设置中文字体（如果需要显示中文）
plt.rcParams['font.sans-serif'] = ['SimHei']  # 用来正常显示中文标签
plt.rcParams['axes.unicode_minus'] = False    # 用来正常显示负号

# 创建图形
plt.figure(figsize=(10, 6))

# 绘制折线图，使用浅紫色三角形标记
plt.plot(network_sizes, false_positive_rates, '-o', color='#B19CD9', linewidth=2, markersize=8, label='HCBF')  # 使用浅紫色
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
plt.xlabel('Edge Network Size', fontsize=12)
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

