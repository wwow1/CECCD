#!/bin/bash

# 从配置文件获取镜像名称
IMAGE_NAME=$(python3 -c "from config import get_image_name; print(get_image_name())")

# 分发到其他节点
for host in $(python3 -c "from config import get_worker_hosts; print(' '.join(h['ip'] for h in get_worker_hosts()))")
do
    echo "Checking image on $host..."
    # 检查远程主机是否已有镜像
    if ! ssh root@$host "docker image inspect $IMAGE_NAME >/dev/null 2>&1"; then
        echo "Image not found on $host, distributing..."
        # 如果是第一次需要分发镜像，才保存
        if [ ! -f edge_cluster.tar ]; then
            echo "Saving image..."
            docker save $IMAGE_NAME -o edge_cluster.tar
        fi
        # 分发镜像
        scp edge_cluster.tar root@$host:/tmp/
        ssh root@$host "docker load -i /tmp/edge_cluster.tar && rm /tmp/edge_cluster.tar"
    else
        echo "Image already exists on $host, skipping..."
    fi
done

# 如果创建了临时文件，清理它
if [ -f edge_cluster.tar ]; then
    echo "Cleaning up temporary files..."
    rm edge_cluster.tar
fi 