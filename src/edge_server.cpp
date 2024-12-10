#include <grpcpp/grpcpp.h>
#include "edge_server.grpc.pb.h"
#include <pqxx/pqxx>
#include <iostream>
#include <thread>
#include <chrono>
#include <map>
#include <vector>
#include <mutex>

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using grpc::ClientContext;
using grpc::Channel;

// 数据库连接信息
const std::string DB_CONNECTION = "host=localhost port=5432 dbname=timescaledb user=postgres password=password";

// 全局访问记录
std::mutex access_records_mutex;
std::vector<AccessRecord> access_records;

// 边缘服务器实现
class EdgeServerImpl final : public EdgeServer::Service {
public:
    // 处理用户SQL请求
    Status ExecuteSQL(ServerContext* context, const SQLRequest* request, SQLResponse* response) override {
        try {
            pqxx::connection conn(DB_CONNECTION);
            pqxx::work txn(conn);

            // 执行查询
            pqxx::result result = txn.exec(request->query());
            std::string formatted_result;
            for (const auto& row : result) {
                for (const auto& field : row) {
                    formatted_result += field.c_str();
                    formatted_result += "\t";
                }
                formatted_result += "\n";
            }
            response->set_result(formatted_result);

            // 记录访问记录
            {
                std::lock_guard<std::mutex> lock(access_records_mutex);
                AccessRecord record;
                record.set_data_source_id(request->data_source_id());
                record.set_timestamp(request->timestamp());
                access_records.push_back(record);
            }

            return Status::OK;
        } catch (const std::exception& e) {
            std::cerr << "SQL Execution Error: " << e.what() << std::endl;
            return Status::CANCELLED;
        }
    }

    // 中心服务器下发缓存更新任务
    Status UpdateCache(ServerContext* context, const CacheUpdateRequest* request, CacheUpdateResponse* response) override {
        try {
            pqxx::connection conn(DB_CONNECTION);
            pqxx::work txn(conn);

            for (const auto& query : request->queries()) {
                txn.exec(query); // 执行缓存更新任务
            }
            txn.commit();
            response->set_status("Cache updated successfully");
            return Status::OK;
        } catch (const std::exception& e) {
            std::cerr << "Cache Update Error: " << e.what() << std::endl;
            return Status::CANCELLED;
        }
    }
};

// 周期性上报访问记录到中心服务器
void ReportStatistics() {
    auto channel = grpc::CreateChannel("central_server_address:50051", grpc::InsecureChannelCredentials());
    auto stub = EdgeServer::NewStub(channel);

    while (true) {
        // 准备访问记录
        std::vector<AccessRecord> records_to_send;
        {
            std::lock_guard<std::mutex> lock(access_records_mutex);
            records_to_send = std::move(access_records); // 清空全局访问记录
            access_records.clear();
        }

        if (!records_to_send.empty()) {
            ClientContext context;
            StatisticsRequest request;

            for (const auto& record : records_to_send) {
                *request.add_access_records() = record; // 添加记录
            }

            StatisticsResponse response;
            Status status = stub->ReportStatistics(&context, request, &response);

            if (!status.ok()) {
                std::cerr << "Failed to report statistics: " << status.error_message() << std::endl;
            } else {
                std::cout << "Reported statistics: " << response.status() << std::endl;
            }
        }

        std::this_thread::sleep_for(std::chrono::minutes(1)); // 每分钟上报一次
    }
}

void RunServer() {
    std::string server_address("0.0.0.0:50052");
    EdgeServerImpl service;

    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::cout << "Edge Server listening on " << server_address << std::endl;
    std::unique_ptr<Server> server(builder.BuildAndStart());

    // 启动上报线程
    std::thread(ReportStatistics).detach();

    server->Wait();
}

int main() {
    RunServer();
    return 0;
}
