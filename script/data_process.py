import pandas as pd
from datetime import datetime

# 读取CSV文件时，将 "?" 识别为 NA
df = pd.read_csv('/root/tmp-data/household_power_consumption.txt', sep=';', na_values=['?'])

# 合并Date和Time列，转换为datetime格式
df['time'] = pd.to_datetime(df['Date'] + ' ' + df['Time'], format='%d/%m/%Y %H:%M:%S')

# 将datetime转换为uint64（Unix时间戳，单位为毫秒）
df['time'] = df['time'].astype('int64') // 10**6  # 转换为毫秒级时间戳

# 删除原来的Date和Time列
df = df.drop(['Date', 'Time'], axis=1)

# 重新排列列的顺序，将data放在第一列
columns = ['time'] + [col for col in df.columns if col != 'time']
df = df[columns]

# 将处理后的数据保存为CSV文件
# 使用空字符串表示缺失值，而不是 'NULL'
df.to_csv('timescaledb_import.csv', index=False, na_rep='')

# 打印前几行查看结果
print(df.head())

# 生成TimescaleDB的COPY命令
table_name = 'household_power_consumption'
columns_str = ', '.join(df.columns)
print("\nTimescaleDB COPY command:")
print(f"\\COPY {table_name}({columns_str}) FROM 'timescaledb_import.csv' CSV HEADER;")