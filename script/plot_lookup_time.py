# -*- coding: utf-8 -*-
import matplotlib.pyplot as plt
import numpy as np

# 数据
network_sizes = ['3x3', '4x4', '5x5', '6x6', '7x7']
# 平均检索时间（ms）
hcbf_avg_times = [13.55, 17.26, 19.88, 21.29, 21.29]
hashmap_avg_times = [11.39, 14.06, 15.58, 16.57, 16.77]
# P99检索时间（ms）
hcbf_p99_times = [29.64, 35.33, 37.35, 38.57, 38.43]
hashmap_p99_times = [20.72, 22.86, 22.75, 22.57, 22.73]

# 绘制平均时间图
plt.figure(figsize=(8, 6))
plt.plot(network_sizes, hcbf_avg_times, '^-', color='#B19CD9', linewidth=2, markersize=8, 
         label='HCBF', alpha=0.9)
plt.plot(network_sizes, hashmap_avg_times, 's-', color='#87CEFA', linewidth=2, markersize=8, 
         label='Unordered Map', alpha=0.9)

# 添加平均时间数据标签
for i in range(len(network_sizes)):
    plt.annotate('{:.2f}'.format(hcbf_avg_times[i]), 
                xy=(i, hcbf_avg_times[i]), 
                xytext=(0, 10),
                textcoords='offset points',
                ha='center',
                va='bottom')
    plt.annotate('{:.2f}'.format(hashmap_avg_times[i]), 
                xy=(i, hashmap_avg_times[i]), 
                xytext=(0, 10),
                textcoords='offset points',
                ha='center',
                va='bottom')

# 设置平均时间图的标题和标签
plt.title('Average Data Discovery Time', fontsize=14, pad=20)
plt.xlabel('Edge Network Size', fontsize=12)
plt.ylabel('Data Discovery Time (ms)', fontsize=12)
plt.legend(loc='upper left', fontsize=10)
plt.grid(True, linestyle='--', alpha=0.7)
plt.ylim(0, max(hcbf_avg_times) * 1.2)

# 优化布局并保存平均时间图
plt.tight_layout()
plt.savefig('avg_lookup_times.png', dpi=300, bbox_inches='tight')
plt.close()

# 绘制P99时间图
plt.figure(figsize=(8, 6))
plt.plot(network_sizes, hcbf_p99_times, '^-', color='#B19CD9', linewidth=2, markersize=8, 
         label='HCBF', alpha=0.9)
plt.plot(network_sizes, hashmap_p99_times, 's-', color='#87CEFA', linewidth=2, markersize=8, 
         label='Unordered Map', alpha=0.9)

# 添加P99时间数据标签
for i in range(len(network_sizes)):
    plt.annotate('{:.2f}'.format(hcbf_p99_times[i]), 
                xy=(i, hcbf_p99_times[i]), 
                xytext=(0, 10),
                textcoords='offset points',
                ha='center',
                va='bottom')
    plt.annotate('{:.2f}'.format(hashmap_p99_times[i]), 
                xy=(i, hashmap_p99_times[i]), 
                xytext=(0, 10),
                textcoords='offset points',
                ha='center',
                va='bottom')

# 设置P99时间图的标题和标签
plt.title('P99 Data Discovery Time', fontsize=14, pad=20)
plt.xlabel('Edge Network Size', fontsize=12)
plt.ylabel('Data Discovery Time (ms)', fontsize=12)
plt.legend(loc='upper left', fontsize=10)
plt.grid(True, linestyle='--', alpha=0.7)
plt.ylim(0, max(hcbf_p99_times) * 1.2)

# 优化布局并保存P99时间图
plt.tight_layout()
plt.savefig('p99_lookup_times.png', dpi=300, bbox_inches='tight')
plt.close() 