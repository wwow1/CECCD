# -*- coding: utf-8 -*-
"""TCN-based Content Popularity Prediction (Clean Version)"""
import numpy as np
import pandas as pd
import torch
import matplotlib.pyplot as plt
from scipy.stats import zipf
from darts import TimeSeries
from darts.models import TCNModel
from darts.dataprocessing.transformers import Scaler
from darts.metrics import mae, mse
import time

# ----------------------
# 1. 数据生成模块
# ----------------------
def generate_zipf_data(num_contents=50000, zipf_param=1.2, time_steps=40):
    """生成符合Zipf分布的模拟数据"""
    popularity = zipf.pmf(np.arange(num_contents)+1, a=zipf_param)
    popularity /= popularity.sum()
    
    time_series = np.random.choice(
        popularity, 
        size=(time_steps, num_contents),
        p=popularity
    ) * np.random.lognormal(0, 0.1, (time_steps, num_contents))
    
    timestamps = pd.date_range('2023-01-01', periods=time_steps, freq='H')
    return pd.DataFrame(
        time_series,
        index=timestamps,
        columns=[f'content_{i}' for i in range(num_contents)]
    )

# ----------------------
# 2. 数据预处理
# ----------------------
full_data = generate_zipf_data()
series = TimeSeries.from_dataframe(full_data, freq='H')
# 标准化处理
scaler = Scaler()
scaled_series = scaler.fit_transform(series)
# 数据集划分
train, val = scaled_series.split_after(0.8)
# 打印训练集的长度
print(f"Length of train dataset: {len(train)}")

# ----------------------
# 3. TCN模型定义
# ----------------------
model = TCNModel(
    input_chunk_length=5,    # 输入历史窗口长度
    output_chunk_length=1,    # 输出预测长度
    kernel_size=2,            # 卷积核尺寸
    num_filters=32,           # 卷积通道数
    num_layers=10,            # 网络深度
    dilation_base=2,          # 扩张系数基数
    dropout=0.1,              # 正则化
    weight_norm=True,         # 权重归一化
    loss_fn=torch.nn.L1Loss() # MAE损失
)

# ----------------------
# 4. 模型训练
# ----------------------
# 记录开始时间
start_time = time.time()

model.fit(
    series=train,
    val_series=val,
    verbose=True
)

# 记录结束时间
end_time = time.time()

# 计算并打印训练所需的时间
training_time = end_time - start_time
print(f"Training took {training_time:.2f} seconds")

# ----------------------
# 5. 模型验证
# ----------------------
# 生成预测
forecast = model.predict(n=1)

# 结果评估
print(f"MAE: {mae(val, forecast):.4f}")
print(f"MSE: {mse(val, forecast):.4f}")

# 可视化对比
plt.figure(figsize=(12, 6))
val[:100].plot(label='True')
forecast[:100].plot(label='Predicted')
plt.title('Popularity Prediction Comparison')
plt.legend()
plt.savefig('prediction_comparison.png')

# ----------------------
# 6. 模型部署接口
# ----------------------
def predict_popularity(new_data: pd.DataFrame) -> np.ndarray:
    """实时预测接口
    """
    # 记录开始时间
    start_time = time.time()

    # 数据预处理
    new_series = TimeSeries.from_dataframe(new_data)
    scaled_series = scaler.transform(new_series)
    
    # 生成预测
    prediction = model.predict(n=1, series=scaled_series)
    
    # 逆标准化
    result = scaler.inverse_transform(prediction).values()

    # 记录结束时间
    end_time = time.time()

    # 计算并打印预测所需的时间
    prediction_time = end_time - start_time
    print(f"Prediction took {prediction_time:.2f} seconds")

    return result

# ----------------------
# 7. 示例使用
# ----------------------
if __name__ == '__main__':
    # 模拟新数据输入
    example_data = generate_zipf_data(time_steps=5)
    
    # 执行预测
    result = predict_popularity(example_data)
    print("Predicted popularity matrix shape:", result.shape)