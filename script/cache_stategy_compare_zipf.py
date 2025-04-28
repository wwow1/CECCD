import numpy as np
import matplotlib.pyplot as plt

zipfian_params = ['0.2', '0.4', '0.6', '0.8', '1.0', '1.2', '1.4', '1.6']
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
lecs_cache_hit_rate = [30.10, 38.87, 50.06, 61.92, 72.73, 82.06, 88.96, 93.07]  # LECS 缓存命中率
trecs_cache_hit_rate = [27.66, 34.78, 46.92, 58.92, 71.37, 81.23, 88.33, 92.79]  # TRECS 缓存命中率

lecs_total_traffic = [131.98, 114.71, 94.72, 76.59, 57.91, 43.80, 32.03, 25.72]  # LECS 总流量(GB)
trecs_total_traffic = [75.78, 74.01, 70.44, 59.60, 47.49, 36.33, 28.34, 23.36]  # TRECS 总流量(GB)

lecs_total_query_time = [18253.631, 16795.568, 14836.376, 12879.077, 10945.809, 9422.303, 8194.911, 7516.688]  # LECS 总查询时间(s)
trecs_total_query_time = [14040.185, 13953.430, 13044.304, 11626.389, 10211.498, 8863.140, 7928.040, 7337.049]  # TRECS 总查询时间(s)

# 定义条形图的索引和宽度
index = np.arange(len(zipfian_params))  # 创建索引
bar_width = 0.35  # 设置条形宽度

# 绘制缓存命中率折线图
plt.figure(figsize=(10, 5))  # 新建图形
axs = plt.gca()  # 获取当前坐标轴
axs.plot(zipfian_params, lecs_cache_hit_rate, label='LECS', marker='^')
axs.plot(zipfian_params, trecs_cache_hit_rate, label='TRECS', marker='o')
axs.set_xlabel('Zipfian分布参数', fontsize=16)
axs.set_ylabel('缓存命中率 (%)', fontsize=16)
axs.legend(fontsize=16)
axs.grid()
plt.savefig('cache_hit_rate_comparison.png', dpi=500, bbox_inches='tight')  # 保存图形
plt.show()  # 显示图形

# 绘制总流量条形图
plt.figure(figsize=(10, 5))
axs = plt.gca()
axs.bar(index, lecs_total_traffic, bar_width, label='LECS')
axs.bar(index + bar_width, trecs_total_traffic, bar_width, label='TRECS',hatch='///')
axs.set_xlabel('Zipfian分布参数', fontsize=16)
axs.set_ylabel('边缘协同网络总流量 (GB)', fontsize=16)
axs.set_xticks(index + bar_width / 2)  # Removed fontsize from here
axs.set_xticklabels(zipfian_params, fontsize=16)  # Kept fontsize here
axs.legend(fontsize=16)
axs.grid()
plt.savefig('total_network_traffic_comparison.png', dpi=500, bbox_inches='tight')  # 保存图形
plt.show()  # 显示图形

# 绘制总查询时间条形图
plt.figure(figsize=(10, 5))
axs = plt.gca()
axs.plot(zipfian_params, lecs_total_query_time, label='LECS', marker='^')
axs.plot(zipfian_params, trecs_total_query_time, label='TRECS', marker='o')
axs.set_xlabel('Zipfian分布参数', fontsize=16)
axs.set_ylabel('总查询时间 (s)', fontsize=16)
axs.set_xticks(index + bar_width / 2)  # Removed fontsize from here
axs.set_xticklabels(zipfian_params, fontsize=16)  # Kept fontsize here
axs.legend(fontsize=16)
axs.grid()
plt.savefig('total_query_time_comparison.png', dpi=500, bbox_inches='tight')  # 保存图形
plt.show()  # 显示图形
