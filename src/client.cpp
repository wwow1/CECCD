#include "client.h"
#include <grpcpp/grpcpp.h>

Client::Client(const std::string& center_server_address) {
    auto channel = grpc::CreateChannel(center_server_address, grpc::InsecureChannelCredentials());
    center_stub_ = cloud_edge_cache::ClientToCenter::NewStub(channel);
    
    if (!channel->WaitForConnected(gpr_time_add(
            gpr_now(GPR_CLOCK_REALTIME),
            gpr_time_from_seconds(5, GPR_TIMESPAN)))) {
        std::cerr << "Failed to connect to center server " << center_server_address << std::endl;
    }
}

void Client::BindToEdgeServer(const std::string& edge_server_address) {
    auto channel = grpc::CreateChannel(edge_server_address, grpc::InsecureChannelCredentials());
    edge_stub_ = cloud_edge_cache::ClientToEdge::NewStub(channel);
    
    if (!channel->WaitForConnected(gpr_time_add(
            gpr_now(GPR_CLOCK_REALTIME),
            gpr_time_from_seconds(5, GPR_TIMESPAN)))) {
        std::cerr << "Failed to connect to edge server " << edge_server_address << std::endl;
    }
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
    grpc::Status status = edge_stub_->Query(&context, request, &response);

    // Handle response
    if (status.ok()) {
        // Check for errors
        if (!response.error().empty()) {
            return "Query error: " + response.error();
        }

        // // Print column headers
        // for (const auto& column : response.columns()) {
        //     std::cout << column.name() << "(" << column.type() << ")\t";
        // }
        // std::cout << std::endl;

        // // Print rows
        // for (const auto& row : response.rows()) {
        //     for (const auto& value : row.values()) {
        //         std::cout << value << "\t";
        //     }
        //     std::cout << std::endl;
        // }
        return "Query success";
    } else {
        return "RPC failed: " + status.error_message();
    }
}

std::vector<std::string> Client::GetEdgeNodes() {
    grpc::ClientContext context;
    cloud_edge_cache::Empty request;
    cloud_edge_cache::EdgeNodesInfo response;
    
    auto status = center_stub_->GetEdgeNodes(&context, request, &response);
    
    if (status.ok()) {
        return {response.edge_nodes().begin(), response.edge_nodes().end()};
    }
    return {};
}
