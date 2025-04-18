# -*- coding: utf-8 -*-
import matplotlib.pyplot as plt
import numpy as np
plt.rcParams.update({
    'font.sans-serif': 'Noto Sans CJK JP',  # 更通用的中文字体
    'axes.unicode_minus': False            # 显示负号
})
# 数据
continue_degree = ['0.2', '0.4', '0.6', '0.8', '1.0']
hcbf_memory_usage = [4600.6, 4600.6, 4600.6, 4600.6, 4600.6]  # HCBF内存占用（MB）
trqindex_memory_usage = [5438.3, 4887.4, 3962.7, 3274.2, 2931.0]  # TRQIndex内存占用（MB）

# 设置条形图的宽度和位置
bar_width = 0.35
x = np.arange(len(continue_degree))

# 创建条形图
plt.figure(figsize=(10, 6))
plt.bar(x - bar_width/2, hcbf_memory_usage, width=bar_width, label='HCBF-Tree', color='#4682B4')
plt.bar(x + bar_width/2, trqindex_memory_usage, width=bar_width, label='TRQIndex', color='#FF6347', hatch='///')

# 设置图表的标题和标签
plt.xlabel('数据块连续性参数(θ)', fontsize=12)
plt.ylabel('边缘数据索引的内存空间占用 (KB)', fontsize=12)
plt.xticks(x, continue_degree)
plt.legend(loc='upper left', fontsize=10)
plt.grid(axis='y', linestyle='--', alpha=0.7)

# 优化布局并保存图表
plt.tight_layout()
plt.savefig('memory_usage_comparison_bar.png', dpi=300, bbox_inches='tight')
plt.close()