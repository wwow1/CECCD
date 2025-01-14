// test_framework/workload_generator.h
#ifndef WORKLOAD_GENERATOR_H
#define WORKLOAD_GENERATOR_H

#include <string>
#include <vector>
#include <random>

class WorkloadGenerator {
public:
    // 生成时间范围查询
    static std::string generateTimeRangeQuery(
        const std::string& stream_id,
        uint64_t start_time,
        uint64_t end_time
    ) {
        return "SELECT * FROM " + stream_id + 
               " WHERE timestamp BETWEEN " + 
               std::to_string(start_time) + " AND " + 
               std::to_string(end_time);
    }
    
    // 生成批量查询
    static std::vector<std::string> generateBatchQueries(
        int batch_size,
        const std::string& stream_id,
        uint64_t time_window
    ) {
        std::vector<std::string> queries;
        uint64_t current_time = std::time(nullptr);
        
        for (int i = 0; i < batch_size; i++) {
            uint64_t start_time = current_time - time_window + 
                                (time_window * i / batch_size);
            uint64_t end_time = start_time + (time_window / batch_size);
            queries.push_back(generateTimeRangeQuery(
                stream_id, start_time, end_time));
        }
        
        return queries;
    }
};

#endif // WORKLOAD_GENERATOR_H