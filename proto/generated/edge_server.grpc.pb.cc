// Generated by the gRPC C++ plugin.
// If you make any local change, they will be lost.
// source: edge_server.proto

#include "edge_server.pb.h"
#include "edge_server.grpc.pb.h"

#include <functional>
#include <grpcpp/support/async_stream.h>
#include <grpcpp/support/async_unary_call.h>
#include <grpcpp/impl/channel_interface.h>
#include <grpcpp/impl/client_unary_call.h>
#include <grpcpp/support/client_callback.h>
#include <grpcpp/support/message_allocator.h>
#include <grpcpp/support/method_handler.h>
#include <grpcpp/impl/rpc_service_method.h>
#include <grpcpp/support/server_callback.h>
#include <grpcpp/impl/server_callback_handlers.h>
#include <grpcpp/server_context.h>
#include <grpcpp/impl/service_type.h>
#include <grpcpp/support/sync_stream.h>

static const char* EdgeServer_method_names[] = {
  "/EdgeServer/ExecuteSQL",
  "/EdgeServer/ReportStatistics",
  "/EdgeServer/UpdateCache",
};

std::unique_ptr< EdgeServer::Stub> EdgeServer::NewStub(const std::shared_ptr< ::grpc::ChannelInterface>& channel, const ::grpc::StubOptions& options) {
  (void)options;
  std::unique_ptr< EdgeServer::Stub> stub(new EdgeServer::Stub(channel, options));
  return stub;
}

EdgeServer::Stub::Stub(const std::shared_ptr< ::grpc::ChannelInterface>& channel, const ::grpc::StubOptions& options)
  : channel_(channel), rpcmethod_ExecuteSQL_(EdgeServer_method_names[0], options.suffix_for_stats(),::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  , rpcmethod_ReportStatistics_(EdgeServer_method_names[1], options.suffix_for_stats(),::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  , rpcmethod_UpdateCache_(EdgeServer_method_names[2], options.suffix_for_stats(),::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  {}

::grpc::Status EdgeServer::Stub::ExecuteSQL(::grpc::ClientContext* context, const ::SQLRequest& request, ::SQLResponse* response) {
  return ::grpc::internal::BlockingUnaryCall< ::SQLRequest, ::SQLResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), rpcmethod_ExecuteSQL_, context, request, response);
}

void EdgeServer::Stub::async::ExecuteSQL(::grpc::ClientContext* context, const ::SQLRequest* request, ::SQLResponse* response, std::function<void(::grpc::Status)> f) {
  ::grpc::internal::CallbackUnaryCall< ::SQLRequest, ::SQLResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_ExecuteSQL_, context, request, response, std::move(f));
}

void EdgeServer::Stub::async::ExecuteSQL(::grpc::ClientContext* context, const ::SQLRequest* request, ::SQLResponse* response, ::grpc::ClientUnaryReactor* reactor) {
  ::grpc::internal::ClientCallbackUnaryFactory::Create< ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_ExecuteSQL_, context, request, response, reactor);
}

::grpc::ClientAsyncResponseReader< ::SQLResponse>* EdgeServer::Stub::PrepareAsyncExecuteSQLRaw(::grpc::ClientContext* context, const ::SQLRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderHelper::Create< ::SQLResponse, ::SQLRequest, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), cq, rpcmethod_ExecuteSQL_, context, request);
}

::grpc::ClientAsyncResponseReader< ::SQLResponse>* EdgeServer::Stub::AsyncExecuteSQLRaw(::grpc::ClientContext* context, const ::SQLRequest& request, ::grpc::CompletionQueue* cq) {
  auto* result =
    this->PrepareAsyncExecuteSQLRaw(context, request, cq);
  result->StartCall();
  return result;
}

::grpc::Status EdgeServer::Stub::ReportStatistics(::grpc::ClientContext* context, const ::StatisticsRequest& request, ::StatisticsResponse* response) {
  return ::grpc::internal::BlockingUnaryCall< ::StatisticsRequest, ::StatisticsResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), rpcmethod_ReportStatistics_, context, request, response);
}

void EdgeServer::Stub::async::ReportStatistics(::grpc::ClientContext* context, const ::StatisticsRequest* request, ::StatisticsResponse* response, std::function<void(::grpc::Status)> f) {
  ::grpc::internal::CallbackUnaryCall< ::StatisticsRequest, ::StatisticsResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_ReportStatistics_, context, request, response, std::move(f));
}

void EdgeServer::Stub::async::ReportStatistics(::grpc::ClientContext* context, const ::StatisticsRequest* request, ::StatisticsResponse* response, ::grpc::ClientUnaryReactor* reactor) {
  ::grpc::internal::ClientCallbackUnaryFactory::Create< ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_ReportStatistics_, context, request, response, reactor);
}

::grpc::ClientAsyncResponseReader< ::StatisticsResponse>* EdgeServer::Stub::PrepareAsyncReportStatisticsRaw(::grpc::ClientContext* context, const ::StatisticsRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderHelper::Create< ::StatisticsResponse, ::StatisticsRequest, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), cq, rpcmethod_ReportStatistics_, context, request);
}

::grpc::ClientAsyncResponseReader< ::StatisticsResponse>* EdgeServer::Stub::AsyncReportStatisticsRaw(::grpc::ClientContext* context, const ::StatisticsRequest& request, ::grpc::CompletionQueue* cq) {
  auto* result =
    this->PrepareAsyncReportStatisticsRaw(context, request, cq);
  result->StartCall();
  return result;
}

::grpc::Status EdgeServer::Stub::UpdateCache(::grpc::ClientContext* context, const ::CacheUpdateRequest& request, ::CacheUpdateResponse* response) {
  return ::grpc::internal::BlockingUnaryCall< ::CacheUpdateRequest, ::CacheUpdateResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), rpcmethod_UpdateCache_, context, request, response);
}

void EdgeServer::Stub::async::UpdateCache(::grpc::ClientContext* context, const ::CacheUpdateRequest* request, ::CacheUpdateResponse* response, std::function<void(::grpc::Status)> f) {
  ::grpc::internal::CallbackUnaryCall< ::CacheUpdateRequest, ::CacheUpdateResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_UpdateCache_, context, request, response, std::move(f));
}

void EdgeServer::Stub::async::UpdateCache(::grpc::ClientContext* context, const ::CacheUpdateRequest* request, ::CacheUpdateResponse* response, ::grpc::ClientUnaryReactor* reactor) {
  ::grpc::internal::ClientCallbackUnaryFactory::Create< ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_UpdateCache_, context, request, response, reactor);
}

::grpc::ClientAsyncResponseReader< ::CacheUpdateResponse>* EdgeServer::Stub::PrepareAsyncUpdateCacheRaw(::grpc::ClientContext* context, const ::CacheUpdateRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderHelper::Create< ::CacheUpdateResponse, ::CacheUpdateRequest, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), cq, rpcmethod_UpdateCache_, context, request);
}

::grpc::ClientAsyncResponseReader< ::CacheUpdateResponse>* EdgeServer::Stub::AsyncUpdateCacheRaw(::grpc::ClientContext* context, const ::CacheUpdateRequest& request, ::grpc::CompletionQueue* cq) {
  auto* result =
    this->PrepareAsyncUpdateCacheRaw(context, request, cq);
  result->StartCall();
  return result;
}

EdgeServer::Service::Service() {
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      EdgeServer_method_names[0],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< EdgeServer::Service, ::SQLRequest, ::SQLResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(
          [](EdgeServer::Service* service,
             ::grpc::ServerContext* ctx,
             const ::SQLRequest* req,
             ::SQLResponse* resp) {
               return service->ExecuteSQL(ctx, req, resp);
             }, this)));
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      EdgeServer_method_names[1],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< EdgeServer::Service, ::StatisticsRequest, ::StatisticsResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(
          [](EdgeServer::Service* service,
             ::grpc::ServerContext* ctx,
             const ::StatisticsRequest* req,
             ::StatisticsResponse* resp) {
               return service->ReportStatistics(ctx, req, resp);
             }, this)));
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      EdgeServer_method_names[2],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< EdgeServer::Service, ::CacheUpdateRequest, ::CacheUpdateResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(
          [](EdgeServer::Service* service,
             ::grpc::ServerContext* ctx,
             const ::CacheUpdateRequest* req,
             ::CacheUpdateResponse* resp) {
               return service->UpdateCache(ctx, req, resp);
             }, this)));
}

EdgeServer::Service::~Service() {
}

::grpc::Status EdgeServer::Service::ExecuteSQL(::grpc::ServerContext* context, const ::SQLRequest* request, ::SQLResponse* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}

::grpc::Status EdgeServer::Service::ReportStatistics(::grpc::ServerContext* context, const ::StatisticsRequest* request, ::StatisticsResponse* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}

::grpc::Status EdgeServer::Service::UpdateCache(::grpc::ServerContext* context, const ::CacheUpdateRequest* request, ::CacheUpdateResponse* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}


