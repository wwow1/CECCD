import numpy as np
import matplotlib.pyplot as plt

cap_params = [512, 1024, 1536, 2048, 2560, 3072, 3584, 4096]
plt.rcParams.update({
    'font.sans-serif': 'Noto Sans CJK JP',  # 更通用的中文字体
    'axes.unicode_minus': False            # 显示负号
})
# 假设的测试结果数据
# 这里的数据需要根据你的实际测试结果进行替换
lecs_cache_hit_rate = [21.97, 34.13, 43.02, 50.03, 55.92, 60.13, 64.18, 67.06]  # LECS 缓存命中率
trecs_cache_hit_rate = [18.60, 30.95, 40.45, 47.20, 52.68, 56.96, 61.86, 64.71]  # TRECS 缓存命中率

lecs_total_traffic = [138.68, 120.17, 105.78, 96.07, 86.35, 78.56, 70.05, 64.92]  # LECS 总流量(GB)
trecs_total_traffic = [120.78, 99.67, 83.57, 70.22, 59.22, 48.96, 41.20, 34.57]  # TRECS 总流量(GB)

lecs_total_query_time = [19563.157, 17613.703, 16009.190, 14914.268, 13789.307, 12874.098, 11981.884, 11276.429]  # LECS 总查询时间(s)
trecs_total_query_time = [18406.918, 16019.806, 14374.472, 12988.839, 11770.786, 10610.166, 9657.666, 8856.419]  # TRECS 总查询时间(s)

# 定义条形图的索引和宽度
index = np.arange(len(cap_params))  # 创建索引
bar_width = 0.35  # 设置条形宽度

# 绘制缓存命中率折线图
plt.figure(figsize=(10, 5))  # 新建图形
axs = plt.gca()  # 获取当前坐标轴
axs.plot(cap_params, lecs_cache_hit_rate, label='LECS', marker='^')
axs.plot(cap_params, trecs_cache_hit_rate, label='TRECS', marker='o')
axs.set_xlabel('边缘服务器的缓存容量')
axs.set_ylabel('缓存命中率 (%)')
axs.set_xticks(cap_params)  # 设置横坐标刻度为 cap_params
axs.set_xticklabels(cap_params)  # 设置横坐标标签为 cap_params
axs.legend()
axs.grid()
plt.savefig('cache_hit_rate_comparison.png', dpi=300, bbox_inches='tight')  # 保存图形
plt.show()  # 显示图形

# 绘制总流量条形图
plt.figure(figsize=(10, 5))  # 新建图形
axs = plt.gca()  # 获取当前坐标轴
axs.bar(index, lecs_total_traffic, bar_width, label='LECS')
axs.bar(index + bar_width, trecs_total_traffic, bar_width, label='TRECS',hatch='///')
axs.set_xlabel('边缘服务器的缓存容量')
axs.set_ylabel('边缘协同网络总流量 (GB)')
axs.set_xticks(index + bar_width / 2)
axs.set_xticklabels(cap_params)
axs.legend()
axs.grid()
plt.savefig('total_network_traffic_comparison.png', dpi=300, bbox_inches='tight')  # 保存图形
plt.show()  # 显示图形

# 绘制总查询时间条形图
plt.figure(figsize=(10, 5))  # 新建图形
axs = plt.gca()  # 获取当前坐标轴
axs.plot(cap_params, lecs_total_query_time, label='LECS', marker='^')
axs.plot(cap_params, trecs_total_query_time, label='TRECS', marker='o')
# axs.bar(index, lecs_total_query_time, bar_width, label='LECS')
# axs.bar(index + bar_width, trecs_total_query_time, bar_width, label='TRECS')
axs.set_xlabel('边缘服务器的缓存容量')
axs.set_ylabel('总查询时间 (s)')
axs.set_xticks(cap_params)  # 设置横坐标刻度为 cap_params
axs.set_xticklabels(cap_params)  # 设置横坐标标签为 cap_params
axs.legend()
axs.grid()
plt.savefig('total_query_time_comparison.png', dpi=300, bbox_inches='tight')  # 保存图形
plt.show()  # 显示图形

# ... existing code ...