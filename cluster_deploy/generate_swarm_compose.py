import yaml
from config import cluster_config, get_image_name, get_manager_host

def generate_swarm_compose():
    num_nodes = len(cluster_config['network']['edge_delays'])
    services = {}
    networks = {}
    
    # 创建overlay网络
    networks["cluster_network"] = {
        "driver": "overlay",
        "attachable": True,
        "ipam": {
            "config": [{"subnet": cluster_config['overlay_network']['subnet']}]
        }
    }
    
    # 配置中心节点
    services["center"] = {
        "image": get_image_name(),
        "networks": {
            "cluster_network": {
                "ipv4_address": cluster_config['overlay_network']['center_ip']
            }
        },
        "environment": [
            f"POSTGRES_PASSWORD={cluster_config['postgres']['password']}",
            f"POSTGRES_USER={cluster_config['postgres']['user']}",
            f"POSTGRES_DB={cluster_config['postgres']['db']}",
            "NODE_ID=0",  # 0表示中心节点
            f"NUM_EDGES={num_nodes}"
        ],
        "cap_add": ["NET_ADMIN"],
        "deploy": {
            "placement": {
                "constraints": [f"node.hostname=={get_manager_host()['name']}"]
            },
            "resources": cluster_config['resources']
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
                f"EDGE_DELAYS={','.join(str(delay) for delay in cluster_config['network']['edge_delays'][i])}"
            ],
            "cap_add": ["NET_ADMIN"], # 允许网络管理
            "deploy": {
                "placement": {
                    "constraints": [f"node.hostname=={host_name}"]
                },
                "resources": cluster_config['resources']
            }
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