// test_framework/test_client.h
#ifndef TEST_CLIENT_H
#define TEST_CLIENT_H

#include "client.h"
#include <chrono>
#include <vector>
#include <string>

struct QueryResult {
    bool success;
    double latency_ms;
    std::string error_message;
    std::string result;
};

class TestClient {
public:
    TestClient(const std::string& server_address) 
        : client_(server_address) {}
    
    QueryResult executeQuery(const std::string& sql_query) {
        QueryResult result;
        
        auto start = std::chrono::high_resolution_clock::now();
        result.result = client_.Query(sql_query);
        auto end = std::chrono::high_resolution_clock::now();
        
        result.latency_ms = std::chrono::duration_cast<std::chrono::milliseconds>
            (end - start).count();
        result.success = result.result.find("Query success") != std::string::npos;
        if (!result.success) {
            result.error_message = result.result;
        }
        
        return result;
    }

private:
    Client client_;
};

#endif // TEST_CLIENT_H