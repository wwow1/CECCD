#!/bin/bash

# 清理现有的swarm集群（如果存在）
echo "Cleaning up existing swarm..."
# 在其他节点上执行leave
for host in $(python3 -c "from config import get_worker_hosts; print(' '.join(h['ip'] for h in get_worker_hosts()))")
do
    ssh root@$host "docker swarm leave || true"
done
# 在管理节点上强制leave
docker swarm leave --force || true

# 初始化新的swarm集群
echo "Initializing new swarm..."
docker swarm init

# 获取join token
JOIN_TOKEN=$(docker swarm join-token worker -q)

# 让其他节点加入集群
echo "Adding workers to swarm..."
for host in $(python3 -c "from config import get_worker_hosts; print(' '.join(h['ip'] for h in get_worker_hosts()))")
do
    ssh root@$host "docker swarm join --token $JOIN_TOKEN 192.168.6.66:2377"
done

echo "Swarm initialization completed!" 