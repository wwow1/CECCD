#!/bin/bash

# 移除stack
docker stack rm edge_cluster

#清理swarm集群（如果需要）
docker swarm leave --force 