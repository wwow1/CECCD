# -*- coding: utf-8 -*-
import matplotlib.pyplot as plt
import numpy as np
import matplotlib.font_manager as fm

# # 检查是否安装了特定的中文字体
# font_families = sorted(set(f.name for f in fm.fontManager.ttflist))
# for font in font_families:
#     print(font)

plt.rcParams.update({
    'font.sans-serif': 'Noto Sans CJK JP',  # 更通用的中文字体
    'axes.unicode_minus': False            # 显示负号
})

# motivation
single_fp = ['3%', '1%', '0.5%', '0.1%']
false_positive_rates = [27.18, 10.47, 5.91, 1.22]

false_positive_rates_trindex = [10.96, 3.62, 0, 0]  # 示例数据

# # 设置中文字体（如果需要显示中文）
# plt.rcParams['font.sans-serif'] = ['Noto Sans CJK JP']  # 用来正常显示中文标签
# plt.rcParams['axes.unicode_minus'] = False    # 用来正常显示负号

# 创建图形
plt.figure(figsize=(10, 6))

# 绘制折线图，使用浅紫色三角形标记
plt.plot(single_fp, false_positive_rates, '-o', color='#B19CD9', linewidth=2, markersize=8, label='HCBF-Tree')  # 使用浅紫色
plt.plot(single_fp, false_positive_rates_trindex, '-^', color='#FF6347', linewidth=2, markersize=8, label='TRQIndex')  # 使用番茄色的实线和圆形标记

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
plt.xlabel('单个计数布隆过滤器的预期假阳性率参数', fontsize=12)
plt.ylabel('边缘数据索引的假阳性率 (%)', fontsize=12)

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

