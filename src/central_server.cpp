#include <grpcpp/grpcpp.h>
#include "edge_server.grpc.pb.h"
#include <iostream>
#include <string>
#include <thread>

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using grpc::ServerWriter;
using grpc::ServerReaderWriter;

using namespace std;

class CentralServerImpl final : public EdgeServer::Service {
public:
    // 接收来自边缘服务器的请求统计
    Status ReportStatistics(ServerContext* context, const StatisticsRequest* request,
                            StatisticsResponse* response) override {
        // 处理接收到的访问记录
        cout << "Received statistics from Edge Server:" << endl;
        for (const auto& record : request->access_records()) {
            cout << "Data Source ID: " << record.data_source_id()
                 << ", Timestamp: " << record.timestamp() << endl;
        }

        response->set_status("Statistics received successfully");
        return Status::OK;
    }

    // 向边缘服务器下发缓存更新任务
    Status UpdateCache(ServerContext* context, const CacheUpdateRequest* request,
                       CacheUpdateResponse* response) override {
        cout << "Received cache update request:" << endl;
        for (const auto& query : request->queries()) {
            cout << "Cache update query: " << query << endl;
        }

        response->set_status("Cache updated successfully");
        return Status::OK;
    }
};

// 启动中心服务器
void RunServer() {
    string server_address("0.0.0.0:50051");
    CentralServerImpl service;

    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    cout << "Central Server listening on " << server_address << endl;
    unique_ptr<Server> server(builder.BuildAndStart());

    server->Wait();
}

int main() {
    RunServer();
    return 0;
}
