import yaml
from config import cluster_config, get_image_name, get_manager_host

def generate_swarm_compose():
    num_nodes = len(cluster_config['network']['edge_delays'])
    services = {}
    networks = {}
    
    # 检查是否使用现有网络
    if cluster_config.get('use_existing_network', False):
        networks["cluster_network"] = {
            "external": True,
            "name": cluster_config['existing_network_name']
        }
    else:
        # 创建新的overlay网络
        networks["cluster_network"] = {
            "driver": "overlay",
            "attachable": True,
            "ipam": {
                "config": [{"subnet": cluster_config['overlay_network']['subnet']}]
            }
        }
    
    # 配置边缘节点
    for i in range(num_nodes):
        service_name = f"edge{i+1}"
        host_name = cluster_config['hosts'][i % len(cluster_config['hosts'])]['name']
        
        services[service_name] = {
            "image": get_image_name(),
            "networks": {
                "cluster_network": {
                    "ipv4_address": f"{cluster_config['overlay_network']['edge_ip_prefix']}.{i+2}"
                }
            },
            "environment": [
                f"POSTGRES_PASSWORD={cluster_config['postgres']['password']}",
                f"POSTGRES_USER={cluster_config['postgres']['user']}",
                f"POSTGRES_DB={cluster_config['postgres']['db']}",
                f"NODE_ID={i+1}",
                f"CENTER_DELAY={cluster_config['network']['center_delay']}",
                f"EDGE_DELAYS={','.join(str(delay) for delay in cluster_config['network']['edge_delays'][i])}",
                f"EDGE_IP={cluster_config['overlay_network']['edge_ip_prefix']}.{i+2}",  # 新增
                "EDGE_PORT=50051"  # 新增
            ],
            "cap_add": ["NET_ADMIN"],
            "deploy": {
                "placement": {
                    "constraints": [f"node.hostname=={host_name}"]
                },
                "resources": cluster_config['resources']
            },
            "volumes": ["/root/TRCEDS:/root/TRCEDS"],  # 新增
            "command": "/root/TRCEDS/build/edge_server $EDGE_IP $EDGE_PORT /root/TRCEDS/config/config.json"  # 新增
        }
    
    # 配置客户端节点
    for i in range(num_nodes):
        service_name = f"client{i+1}"
        host_name = cluster_config['hosts'][i % len(cluster_config['hosts'])]['name']
        
        services[service_name] = {
            "image": get_image_name(),
            "networks": {
                "cluster_network": {
                    "ipv4_address": f"{cluster_config['overlay_network']['edge_ip_prefix']}.{i+num_nodes+2}"
                }
            },
            "environment": [
                f"POSTGRES_PASSWORD={cluster_config['postgres']['password']}",
                f"POSTGRES_USER={cluster_config['postgres']['user']}",
                f"POSTGRES_DB={cluster_config['postgres']['db']}",
                f"NODE_ID={i+num_nodes+1}"
            ],
            "cap_add": ["NET_ADMIN"],
            "deploy": {
                "placement": {
                    "constraints": [f"node.hostname=={host_name}"]
                },
                "resources": cluster_config['resources']
            }
        }
    
    compose_dict = {
        "version": "3.8",
        "services": services,
        "networks": networks
    }
    
    with open("docker-stack.yml", "w") as file:
        yaml.dump(compose_dict, file)

# Example usage
generate_swarm_compose()