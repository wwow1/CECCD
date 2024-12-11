#include "client.h"

Client::Client(const std::string& server_address) {
    stub_ = cloud_edge_cache::ClientToEdge::NewStub(
        grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials()));
}

std::string Client::Query(const std::string& sql_query) {
    // Prepare request
    cloud_edge_cache::QueryRequest request;
    request.set_sql_query(sql_query);

    // Response container
    cloud_edge_cache::QueryResponse response;

    // Context for the client
    grpc::ClientContext context;

    // Make the RPC call
    grpc::Status status = stub_->Query(&context, request, &response);

    // Handle response
    if (status.ok()) {
        return response.result();
    } else {
        return "RPC failed: " + status.error_message();
    }
}
