# -*- coding: utf-8 -*-
import matplotlib.pyplot as plt
import numpy as np
plt.rcParams.update({
    'font.sans-serif': 'Noto Sans CJK JP',
    'axes.unicode_minus': False,
    'legend.fontsize': 16,       # 图例字体大小
    'axes.labelsize': 16,       # 坐标轴标签大小
    'xtick.labelsize': 16,      # X轴刻度大小
    'ytick.labelsize': 16       # Y轴刻度大小
})
# 数据
single_fp = ['3%', '1%', '0.5%', '0.1%']
hcbf_memory_usage = [3503.1, 4600.6, 5290.3, 6900.1]  # HCBF内存占用（MB）
trqindex_memory_usage = [4236.4, 4887.4, 5134.5, 5134.5]  # TRQIndex内存占用（MB）

# 设置条形图的宽度和位置
bar_width = 0.35
x = np.arange(len(single_fp))

# 创建条形图
plt.figure(figsize=(10, 6))
plt.bar(x - bar_width/2, hcbf_memory_usage, width=bar_width, label='HCBF-Tree', color='#4682B4')
plt.bar(x + bar_width/2, trqindex_memory_usage, width=bar_width, label='TRQIndex', color='#FF6347',hatch='///')

# 设置图表的标题和标签
# 修改图表标签设置
plt.xlabel('单个计数布隆过滤器的预期假阳性率参数', fontsize=16)
plt.ylabel('边缘数据索引的内存空间占用 (KB)', fontsize=16)
plt.xticks(x, single_fp, fontsize=16)  # 添加X轴刻度标签大小
plt.yticks(fontsize=16)  # 添加Y轴刻度标签大小
plt.legend(loc='upper left', fontsize=16)  # 修改图例字体大小
plt.grid(axis='y', linestyle='--', alpha=0.7)

# 优化布局并保存图表
plt.tight_layout()
plt.savefig('memory_usage_comparison_bar.png', dpi=500, bbox_inches='tight')
plt.close()