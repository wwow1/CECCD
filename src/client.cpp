#include <grpcpp/grpcpp.h>
#include "edge_server.grpc.pb.h"
#include <iostream>
#include <string>

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;

using namespace std;

class EdgeClient {
public:
    EdgeClient(std::shared_ptr<Channel> channel)
        : stub_(EdgeServer::NewStub(channel)) {}

    // 执行 SQL 查询并返回结果
    string ExecuteSQL(const string& query, const string& data_source_id, int64_t timestamp) {
        SQLRequest request;
        request.set_query(query);
        request.set_data_source_id(data_source_id);
        request.set_timestamp(timestamp);

        SQLResponse response;
        ClientContext context;

        Status status = stub_->ExecuteSQL(&context, request, &response);

        if (status.ok()) {
            return response.result();
        } else {
            cerr << "RPC failed: " << status.error_message() << endl;
            return "";
        }
    }

private:
    std::unique_ptr<EdgeServer::Stub> stub_;
};

int main(int argc, char** argv) {
    if (argc < 4) {
        cerr << "Usage: " << argv[0] << " <server_address> <data_source_id> <timestamp>" << endl;
        return -1;
    }

    string server_address = argv[1];
    string data_source_id = argv[2];
    int64_t timestamp = std::stoll(argv[3]);

    EdgeClient client(grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials()));

    string query;
    cout << "Enter SQL query: ";
    getline(cin, query);

    string result = client.ExecuteSQL(query, data_source_id, timestamp);

    if (!result.empty()) {
        cout << "Query result:\n" << result << endl;
    }

    return 0;
}
