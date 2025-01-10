#!/bin/bash

# 从参数中获取中心节点和边缘节点的IP信息
CENTER_IP=$1
shift
declare -A EDGE_IPS
for pair in "$@"; do
    node_num=${pair%%:*}
    ip=${pair#*:}
    EDGE_IPS[$node_num]=$ip
done

# 从配置文件获取延迟信息
eval "$(python3 -c "
from config import cluster_config
print(f'CENTER_DELAY={cluster_config[\"network\"][\"center_delay\"]}')
print('declare -a MATRIX=(')
for row in cluster_config['network']['edge_delays']:
    print(f'    \"{\" \".join(map(str, row))}\"')
print(')')
")"

echo "CENTER_DELAY: $CENTER_DELAY"
declare -a MATRIX=("${MATRIX[@]}")

# 打印调试信息
echo "Debug: Received matrix content:"
echo "MATRIX length: ${#MATRIX[@]}"
for i in "${!MATRIX[@]}"; do
    echo "MATRIX[$i]: ${MATRIX[$i]}"
done

echo "========================================="
echo "Starting network delay configuration"
echo "========================================="

# 输出中心节点和边缘节点的编号及其 IP 地址
echo "Center Node IP: $CENTER_IP"
for node_num in "${!EDGE_IPS[@]}"; do
    echo "Edge Node $node_num IP: ${EDGE_IPS[$node_num]}"
done

# 获取本地容器
LOCAL_CONTAINERS=$(docker ps --format '{{.Names}}' | grep "edge_cluster_edge\|edge_cluster_center" || true)

if [ ! -z "$LOCAL_CONTAINERS" ]; then
    echo "Found local containers: $LOCAL_CONTAINERS"
else
    echo "No local containers found on this host"
    exit 0
fi

for container in $LOCAL_CONTAINERS; do
    echo ""
    echo "----------------------------------------"
    echo "Configuring container: $container"
    echo "----------------------------------------"
    
    # 判断是中心节点还是边缘节点
    if [[ "$container" == *"center"* ]]; then
        echo "Container is center node"
        
        # 设置基础tc规则
        INTERFACE=$(docker exec "$container" sh -c "ip route show default" | awk '{print $5}')
        # 如果 eth0 是主要接口，确保在 eth0 上应用规则
        if [ "$INTERFACE" != "eth0" ]; then
            INTERFACE="eth0"
        fi
        echo "Using network interface: $INTERFACE"
        
        echo "Cleaning up existing tc rules..."
        docker exec "$container" tc qdisc del dev "$INTERFACE" root || true
        
        echo "Setting up tc infrastructure..."
        docker exec "$container" tc qdisc add dev "$INTERFACE" root handle 1: prio bands 8
        
        # 配置到所有边缘节点的延迟
        rule_id=1
        for edge_id in "${!EDGE_IPS[@]}"; do
            edge_ip=${EDGE_IPS[$edge_id]}
            echo "Configuring delay to edge$edge_id:"
            echo "  - From: center ($container)"
            echo "  - To: edge$edge_id ($edge_ip)"
            echo "  - Delay: ${CENTER_DELAY}ms"
            
            docker exec "$container" tc qdisc add dev "$INTERFACE" parent 1:$rule_id handle "${rule_id}0:" netem delay "${CENTER_DELAY}ms"
            docker exec "$container" tc filter add dev "$INTERFACE" protocol ip parent 1:0 prio 1 u32 match ip dst "$edge_ip" flowid 1:$rule_id
            echo "✓ Delay to edge$edge_id configured"
            
            rule_id=$((rule_id + 1))
        done
    else
        # 边缘节点的配置（原有代码）
        node_id=$(echo "$container" | grep -o "edge[0-9]*" | grep -o "[0-9]*")
        echo "Container is edge node $node_id"
        
        # 设置基础tc规则
        INTERFACE=$(docker exec "$container" sh -c "ip route show default" | awk '{print $5}')
        # 如果 eth0 是主要接口，确保在 eth0 上应用规则
        if [ "$INTERFACE" != "eth0" ]; then
            INTERFACE="eth0"
        fi
        echo "Using network interface: $INTERFACE"
        
        echo "Cleaning up existing tc rules..."
        docker exec "$container" tc qdisc del dev "$INTERFACE" root || true
        
        echo "Setting up tc infrastructure..."
        docker exec "$container" tc qdisc add dev "$INTERFACE" root handle 1: prio bands 8
        
        # 配置到中心节点的延迟
        if [ ! -z "$CENTER_IP" ]; then
            echo "Configuring delay to center node:"
            echo "  - From: edge$node_id ($container)"
            echo "  - To: center ($CENTER_IP)"
            echo "  - Delay: ${CENTER_DELAY}ms"
            docker exec "$container" tc qdisc add dev "$INTERFACE" parent 1:1 handle 10: netem delay "${CENTER_DELAY}ms"
            docker exec "$container" tc filter add dev "$INTERFACE" protocol ip parent 1:0 prio 1 u32 match ip dst "$CENTER_IP" flowid 1:1
            echo "✓ Delay to center node configured"
        fi
        
        # 配置到其他边缘节点的延迟
        rule_id=2
        for other_id in "${!EDGE_IPS[@]}"; do
            if [ "$node_id" != "$other_id" ]; then
                other_ip=${EDGE_IPS[$other_id]}
                delay=${MATRIX[$((node_id-1))]}
                delay=$(echo "$delay" | cut -d' ' -f$((other_id)))
                
                # 添加调试信息
                echo "Debug info:"
                echo "  - Matrix content: ${MATRIX[*]}"
                echo "  - Selected row: ${MATRIX[$((node_id-1))]}"
                echo "  - Calculated delay: $delay"
                
                # 确保delay不为空
                if [ -z "$delay" ]; then
                    echo "Error: Delay value is empty!"
                    continue
                fi
                
                echo "Configuring delay to edge$other_id:"
                echo "  - From: edge$node_id ($container)"
                echo "  - To: edge$other_id ($other_ip)"
                echo "  - Delay: ${delay}ms"
                
                docker exec "$container" tc qdisc add dev "$INTERFACE" parent 1:$rule_id handle "${rule_id}0:" netem delay "${delay}ms"
                docker exec "$container" tc filter add dev "$INTERFACE" protocol ip parent 1:0 prio 1 u32 match ip dst "$other_ip" flowid 1:$rule_id
                echo "✓ Delay to edge$other_id configured"
                
                rule_id=$((rule_id + 1))
            fi
        done
    fi
    
    echo "Verifying configuration for $container:"
    echo "TC rules:"
    docker exec "$container" tc qdisc show dev "$INTERFACE"
    echo "TC filters:"
    docker exec "$container" tc filter show dev "$INTERFACE"
done

echo ""
echo "========================================="
echo "Network delay configuration completed"
echo "=========================================" 