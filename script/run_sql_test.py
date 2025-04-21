import subprocess
import time

# 定义参数列表
params_list = [
    ["172.18.0.3", "54321", "1704038400000", "1704044400000", "20000", "1499", "0", "25", "0.8"],  # 新参数顺序
    ["172.18.0.4", "54321", "1704038400000", "1704044400000", "20000", "1499", "1", "25", "0.8"]   # 新参数顺序
]

# 修改为循环1000次
for _ in range(1000):
    for params in params_list:
        print(f"Running sql_test with parameters: {' '.join(params)}")
        try:
            subprocess.run(["/root/CECCD/build/sql_test"] + params, check=True)
        except subprocess.CalledProcessError as e:
            print(f"Error occurred: {e}")
        # 可以根据需要添加每次调用之间的延迟，单位为秒
        # time.sleep(1)

# 移除持续循环
# while True:
    for params in params_list:
        print(f"Running sql_test with parameters: {' '.join(params)}")
        try:
            subprocess.run(["/root/CECCD/build/sql_test"] + params, check=True)
        except subprocess.CalledProcessError as e:
            print(f"Error occurred: {e}")
        # 可以根据需要添加每次调用之间的延迟，单位为秒
        # time.sleep(1)