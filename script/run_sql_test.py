import subprocess
import time
import sys

# 检查是否有用户输入参数
if len(sys.argv) < 2:
    print("Usage: python run_sql_test.py <server_ip>")
    sys.exit(1)

# 使用用户输入的第一个参数作为服务器IP
server_ip = sys.argv[1]

# 定义参数列表，使用用户输入的server_ip
params_list = [
    [server_ip, "54321", "1704038415000", "1705538386000", "86400", "1499", "0", "25", "1.0"],
]

# 修改为循环1000次
for _ in range(1000):
    for params in params_list:
        print(f"Running sql_test with parameters: {' '.join(params)}")
        try:
            subprocess.run(["/root/TRCEDS/build/sql_test"] + params, check=True)
        except subprocess.CalledProcessError as e:
            print(f"Error occurred: {e}")
        # 可以根据需要添加每次调用之间的延迟，单位为秒
        # time.sleep(1)
time.sleep(20)
# 修改为循环1000次
for _ in range(1000):
    for params in params_list:
        print(f"Running sql_test with parameters: {' '.join(params)}")
        try:
            subprocess.run(["/root/TRCEDS/build/sql_test"] + params, check=True)
        except subprocess.CalledProcessError as e:
            print(f"Error occurred: {e}")