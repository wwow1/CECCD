#!/bin/bash

# 从配置文件获取信息
eval "$(python3 -c "
from config import cluster_config, get_worker_hosts, get_manager_host
print(f'MANAGER_IP={get_manager_host()[\"ip\"]}')
print(f'MANAGER_USER={get_manager_host()[\"ssh_user\"]}')
")"

# 获取中心节点和边缘节点的IP信息
CENTER_CONTAINER_ID=$(docker service ps edge_cluster_center -q --no-trunc --filter "desired-state=running" | head -n1)
CENTER_IP=$(docker inspect "$CENTER_CONTAINER_ID" --format '{{range .NetworksAttachments}}{{range .Addresses}}{{.}}{{end}}{{end}}' | cut -d'/' -f1)

declare -A EDGE_IPS
for service in $(docker service ls --format "{{.Name}}" | grep "edge_cluster_edge"); do
    node_num=$(echo "$service" | grep -o '[0-9]*$')
    container_id=$(docker service ps "$service" -q --no-trunc --filter "desired-state=running" | head -n1)
    if [ ! -z "$container_id" ]; then
        ip=$(docker inspect "$container_id" --format '{{range .NetworksAttachments}}{{range .Addresses}}{{.}}{{end}}{{end}}' | cut -d'/' -f1)
        if [ ! -z "$ip" ] && [ ! -z "$node_num" ]; then
            EDGE_IPS[$node_num]=$ip
        fi
    fi
done

# 修改脚本权限
chmod +x setup_delay.sh

# 分发并执行脚本到所有节点
echo "Starting configuration on all hosts..."
for host in $(python3 -c "from config import cluster_config; print(' '.join(h['ip'] for h in cluster_config['hosts']))"); do
    echo ""
    echo "----------------------------------------"
    echo "Configuring host: $host"
    echo "----------------------------------------"
    
    # 分发脚本和配置文件
    scp setup_delay.sh "root@$host:/tmp/"
    scp config.py "root@$host:/tmp/"
    
    # 构建EDGE_IPS字符串，确保按顺序
    edge_ips_str=""
    for node_num in $(seq 1 ${#EDGE_IPS[@]}); do
        if [ -n "${EDGE_IPS[$node_num]}" ]; then
            edge_ips_str+="$node_num:${EDGE_IPS[$node_num]} "
        fi
    done
    
    # 使用Here文档方式执行远程命令
    ssh "root@$host" /bin/bash << EOF
        cd /tmp
        bash setup_delay.sh ${CENTER_IP} ${edge_ips_str}
EOF
done

echo ""
echo "========================================="
echo "Configuration completed on all hosts"
echo "========================================="