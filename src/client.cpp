#include "client.h"

Client::Client(const std::string& server_address) {
    auto channel = grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials());
    stub_ = cloud_edge_cache::ClientToEdge::NewStub(channel);
    // 等待目标节点通道就绪
    if (!channel->WaitForConnected(gpr_time_add(
            gpr_now(GPR_CLOCK_REALTIME),
            gpr_time_from_seconds(5, GPR_TIMESPAN)))) {
        std::cerr << "Failed to connect to target node " << server_address << std::endl;
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
    grpc::Status status = stub_->Query(&context, request, &response);

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
