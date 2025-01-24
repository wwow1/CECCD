# -*- coding: utf-8 -*-
import matplotlib.pyplot as plt
import numpy as np

# 数据
network_sizes = ['3x3', '4x4', '5x5', '6x6', '7x7']
false_positive_rates = [3.21, 6.02, 9.52, 13.33, 18.05]

# 设置中文字体（如果需要显示中文）
plt.rcParams['font.sans-serif'] = ['SimHei']  # 用来正常显示中文标签
plt.rcParams['axes.unicode_minus'] = False    # 用来正常显示负号

# 创建图形
plt.figure(figsize=(10, 6))

# 绘制折线图，使用浅紫色三角形标记
plt.plot(network_sizes, false_positive_rates, '^-', color='#B19CD9', linewidth=2, markersize=8)  # 使用浅紫色

# 添加数据标签
for i, rate in enumerate(false_positive_rates):
    plt.annotate('{:.2f}%'.format(rate), 
                xy=(i, rate), 
                xytext=(0, 10),
                textcoords='offset points',
                ha='center',
                va='bottom')

# 设置标题和标签
plt.title('False Positive Rate vs Edge Network Size', fontsize=14, pad=20)
plt.xlabel('Edge Network Size', fontsize=12)
plt.ylabel('False Positive Rate (%)', fontsize=12)

# 设置网格
plt.grid(True, linestyle='--', alpha=0.7)

# 设置y轴范围，留出一些空间显示数据标签
plt.ylim(0, max(false_positive_rates) * 1.2)

# 优化布局
plt.tight_layout()

# 保存图片
plt.savefig('false_positive_rates.png', dpi=300, bbox_inches='tight')
plt.close()

