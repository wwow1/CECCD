# 集群配置
cluster_config = {
    # Docker镜像配置
    "image": {
        "name": "edge_cluster",
        "tag": "1"
    },
    
    # 容器资源限制
    "resources": {
        "limits": {
            "cpus": "2",
            "memory": "1G"
        },
        "reservations": {
            "cpus": "1",
            "memory": "512M"
        }
    },
    
    # 数据库配置
    "postgres": {
        "password": "password",
        "user": "postgres",
        "db": "postgres"
    },
    
    # 主机配置
    "hosts": [
        {
            "name": "ca66",
            "ip": "192.168.6.66",
            "ssh_user": "root",
            "role": "manager"
        },
        {
            "name": "ca68",
            "ip": "192.168.6.68",
            "ssh_user": "root",
            "role": "worker"  
        },
        {
            "name": "ca95",
            "ip": "192.168.6.95",
            "ssh_user": "zfy",
            "role": "worker"  # manager或worker
        },
        {
            "name": "ca96",
            "ip": "192.168.6.96",
            "ssh_user": "root",
            "role": "worker"
        },
        {
            "name": "ca42",
            "ip": "192.168.6.42",
            "ssh_user": "zfy",
            "role": "worker"
        },
        {
            "name": "ca56",
            "ip": "192.168.6.56",
            "ssh_user": "zfy",
            "role": "worker"
        },
    ],
    "num_nodes": 25,
    # 网络延迟配置（单位：毫秒）
    "network": {
        # 边缘节点之间的延迟矩阵
        "edge_delays": [
            [0,   10,   15,  20],  # 节点1到其他节点的延迟
            [10,  0,    15,  20],  # 节点2到其他节点的延迟
            [15,  15,   0,   30],  # 节点3到其他节点的延迟
            [20,  20,   30,  0],   # 节点4到其他节点的延迟
        ],
        # 边缘节点到中心节点的延迟
        "center_delay": 50
    },
    
    # 网络配置
    "overlay_network": {
        "subnet": "172.20.0.0/16",
        "center_ip": "172.20.0.2",
        "edge_ip_prefix": "172.20.1",
        'use_existing_network': True,  # 设置为True表示使用现有网络
        'existing_network_name': 'cluster_network',  # 现有网络名称
    }
}

# 辅助函数
def get_image_name():
    return f"{cluster_config['image']['name']}:{cluster_config['image']['tag']}"

def get_manager_host():
    return next(host for host in cluster_config['hosts'] if host['role'] == 'manager')

def get_worker_hosts():
    return [host for host in cluster_config['hosts'] if host['role'] == 'worker']

def validate_config():
    # 验证延迟矩阵
    delays = cluster_config['network']['edge_delays']
    n = len(delays)
    
    # 检查是否为方阵
    if not all(len(row) == n for row in delays):
        raise ValueError("Delay matrix must be square")
    
    # 检查对角线是否为0
    if not all(delays[i][i] == 0 for i in range(n)):
        raise ValueError("Diagonal elements must be 0")
    
    # 检查是否对称
    if not all(delays[i][j] == delays[j][i] 
              for i in range(n) for j in range(n)):
        raise ValueError("Delay matrix must be symmetric")
    
    # 检查是否有且仅有一个manager
    manager_count = sum(1 for host in cluster_config['hosts'] if host['role'] == 'manager')
    if manager_count != 1:
        raise ValueError("Must have exactly one manager node")

# 在导入时验证配置
validate_config()