#!/bin/bash

# 初始化swarm
bash init_swarm.sh

# 生成stack配置文件
python3 generate_swarm_compose.py

# 分发镜像到所有节点
bash distribute_image.sh

# 部署stack
docker stack deploy -c docker-stack.yml edge_cluster

# 等待服务启动
echo "Waiting for services to start..."
sleep 45

# 配置网络延迟
#bash configure_network_delay.sh