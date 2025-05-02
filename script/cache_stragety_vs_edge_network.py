import numpy as np
import matplotlib.pyplot as plt

network_sizes = ['5x5', '6x6', '7x7', '8x8', '9x9', '10x10']
plt.rcParams.update({
    'font.sans-serif': 'Noto Sans CJK JP',
    'axes.unicode_minus': False,
    'legend.fontsize': 16,       # 图例字体大小
    'axes.labelsize': 16,       # 坐标轴标签大小
    'xtick.labelsize': 16,      # X轴刻度大小
    'ytick.labelsize': 16       # Y轴刻度大小
})
# 假设的测试结果数据
# 这里的数据需要根据你的实际测试结果进行替换
lecs_cache_hit_rate = [50.03, 49.33, 48.92, 48.57, 47.99, 47.79]  # LECS 缓存命中率
trecs_cache_hit_rate = [47.25, 46.34, 46.13, 45.69, 45.20, 44.61]  # TRECS 缓存命中率

lecs_total_traffic = [98.55, 142.18, 196.29, 254.80, 324.96, 403.33]  # LECS 总流量(GB)
trecs_total_traffic = [71.36, 106.61, 148.17, 192.75, 246.26, 302.06]  # TRECS 总流量(GB)

lecs_total_query_time = [14927.767, 21607.748, 29892.118, 39108.687, 49717.441, 61820.738]  # LECS 总查询时间(s)
trecs_total_query_time = [13183.860, 18887.447, 26372.258, 34406.822, 44027.214, 54106.375]  # TRECS 总查询时间(s)

# 定义条形图的索引和宽度
index = np.arange(len(network_sizes))  # 创建索引
bar_width = 0.35  # 设置条形宽度

# 绘制缓存命中率折线图
plt.figure(figsize=(10, 5))  # 新建图形
axs = plt.gca()  # 获取当前坐标轴
axs.plot(network_sizes, lecs_cache_hit_rate, label='LECS', marker='^')
axs.plot(network_sizes, trecs_cache_hit_rate, label='TRECS', marker='o')
axs.set_xlabel('边缘网络规模', fontsize=16)
axs.set_ylabel('缓存命中率 (%)', fontsize=16)
axs.legend(fontsize=16)
axs.set_ylim(30, 70)  # 新增这行设置y轴上限为30
axs.grid()
plt.savefig('cache_hit_rate_comparison.png', dpi=500, bbox_inches='tight')  # 保存图形
plt.show()  # 显示图形

# 绘制总流量条形图
plt.figure(figsize=(10, 5))
axs = plt.gca()
axs.bar(index, lecs_total_traffic, bar_width, label='LECS')
axs.bar(index + bar_width, trecs_total_traffic, bar_width, label='TRECS',hatch='///')
axs.set_xlabel('边缘网络规模', fontsize=16)
axs.set_ylabel('边缘协同网络总流量 (GB)', fontsize=16)
axs.set_xticks(index + bar_width / 2)  # Removed fontsize from here
axs.set_xticklabels(network_sizes, fontsize=16)  # Kept fontsize here
axs.legend(fontsize=16)
axs.grid()
plt.savefig('total_network_traffic_comparison.png', dpi=500, bbox_inches='tight')  # 保存图形
plt.show()  # 显示图形

# 绘制总查询时间条形图
plt.figure(figsize=(10, 5))
axs = plt.gca()
axs.plot(network_sizes, lecs_total_query_time, label='LECS', marker='^')
axs.plot(network_sizes, trecs_total_query_time, label='TRECS', marker='o')
axs.set_xlabel('边缘网络规模', fontsize=16)
axs.set_ylabel('总查询时间 (s)', fontsize=16)
axs.set_xticks(index + bar_width / 2)  # Removed fontsize from here
axs.set_xticklabels(network_sizes, fontsize=16)  # Kept fontsize here
axs.legend(fontsize=16)
axs.grid()
plt.savefig('total_query_time_comparison.png', dpi=500, bbox_inches='tight')  # 保存图形
plt.show()  # 显示图形
