cmake_minimum_required(VERSION 3.10)
project(cloud_edge_cache)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置项目的 include 目录
include_directories(
    ${PROJECT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}
)

# Find required packages
find_package(Protobuf REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(TBB REQUIRED)

# 添加这些变量定义
set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:gRPC::grpc_cpp_plugin>)
set(_PROTOBUF_PROTOC $<TARGET_FILE:protobuf::protoc>)

# Generate protobuf and gRPC code
get_filename_component(proto_file "proto/cloud_edge_cache.proto" ABSOLUTE)
get_filename_component(proto_path "${proto_file}" PATH)

# Generate both protobuf and gRPC files
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/cloud_edge_cache.pb.cc"
           "${CMAKE_CURRENT_BINARY_DIR}/cloud_edge_cache.pb.h"
           "${CMAKE_CURRENT_BINARY_DIR}/cloud_edge_cache.grpc.pb.cc"
           "${CMAKE_CURRENT_BINARY_DIR}/cloud_edge_cache.grpc.pb.h"
    COMMAND ${_PROTOBUF_PROTOC}
    ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
         --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
         -I "${proto_path}"
         --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
         "${proto_file}"
    DEPENDS "${proto_file}")

# Update source files variables
set(PROTO_SRCS
    "${CMAKE_CURRENT_BINARY_DIR}/cloud_edge_cache.pb.cc"
    "${CMAKE_CURRENT_BINARY_DIR}/cloud_edge_cache.grpc.pb.cc")
set(PROTO_HDRS
    "${CMAKE_CURRENT_BINARY_DIR}/cloud_edge_cache.pb.h"
    "${CMAKE_CURRENT_BINARY_DIR}/cloud_edge_cache.grpc.pb.h")

# Common source files
set(COMMON_SOURCES
    src/config_manager.cpp
    src/edge_cache_index.cpp
)

# 添加 json 库
add_subdirectory(third_party/json)

# 为 libpqxx 添加配置
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries")
set(SKIP_BUILD_TEST ON CACHE BOOL "Skip build test")
add_subdirectory(third_party/libpqxx)

# 为 sql-parser 添加配置
set(ENABLE_TESTS OFF CACHE BOOL "Disable tests for sql-parser")
add_subdirectory(third_party/sql-parser)

# Client static library
add_library(client STATIC
    src/client.cpp
    ${COMMON_SOURCES}
    ${PROTO_SRCS}
    ${PROTO_HDRS}
)
target_link_libraries(client
    ${PROTOBUF_LIBRARIES}
    ${GRPC_LIBRARIES}
    gRPC::grpc++
    TBB::tbb
    nlohmann_json::nlohmann_json
    pqxx
    sql-parser
)
target_include_directories(client
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
        ${PROTOBUF_INCLUDE_DIRS}
        ${GRPC_INCLUDE_DIRS}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/sql-parser/src
)

# Edge server executable
add_executable(edge_server
    src/edge_server.cpp
    ${COMMON_SOURCES}
    ${PROTO_SRCS}
    ${PROTO_HDRS}
)
target_link_libraries(edge_server
    ${PROTOBUF_LIBRARIES}
    ${GRPC_LIBRARIES}
    gRPC::grpc++
    TBB::tbb
    pqxx
    sql-parser
    nlohmann_json::nlohmann_json
)
target_include_directories(edge_server
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
        ${PROTOBUF_INCLUDE_DIRS}
        ${GRPC_INCLUDE_DIRS}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/sql-parser/src
)

# Center server executable
# add_executable(center_server
#     src/central_server.cpp
#     ${COMMON_SOURCES}
#     ${PROTO_SRCS}
#     ${PROTO_HDRS}
#     ${GRPC_SRCS}
#     ${GRPC_HDRS}
# )
# target_link_libraries(center_server
#     ${PROTOBUF_LIBRARIES}
#     ${GRPC_LIBRARIES}
#     gRPC::grpc++
#     TBB::tbb
# )
# target_include_directories(center_server
#     PUBLIC
#         ${CMAKE_CURRENT_SOURCE_DIR}
#         ${CMAKE_CURRENT_BINARY_DIR}
#         ${PROTOBUF_INCLUDE_DIRS}
#         ${GRPC_INCLUDE_DIRS}
#     PRIVATE
#         ${CMAKE_CURRENT_SOURCE_DIR}/include
# )

# Installation rules
install(TARGETS client
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(TARGETS edge_server #center_server
    RUNTIME DESTINATION bin
)

install(FILES
    include/client.h
    include/config_manager.h
    include/common.h
    DESTINATION include/cloud_edge_cache
)