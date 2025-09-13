#include "StatusGrpcClient.h"
#include "ConfigManager.h"
#include "Logger.h"

message::GetChatServerResponse StatusGrpcClient::GetChatServer(std::string uid)
{
	grpc::ClientContext context;
	message::GetChatServerResponse response;
	message::GetChatServerRequest request;

	request.set_uid(uid);
	auto stub = _pool->GetConnection();
	LOG_INFO("Calling GetChatServer for uid={}", uid);
	auto status = stub->GetChatServer(&context, request, &response);

	if (!status.ok()) {
		LOG_ERROR("gRPC GetChatServer failed: {} ", status.error_message());
		response.set_error(static_cast<int>(ErrorCodes::RPC_FAILED));
	}
	else {
		LOG_DEBUG("gRPC GetChatServer succeeded for uid={}", uid);
	}
	_pool->ReturnConnection(std::move(stub));
	return response;
}

message::LoginResponse StatusGrpcClient::Login(std::string uid, std::string token)
{
	grpc::ClientContext context;
	message::LoginResponse response;
	message::LoginRequest request;

	request.set_uid(uid);
	request.set_token(token);

	auto stub = _pool->GetConnection();
	LOG_INFO("Calling Login for uid={}", uid);
	auto status = stub->Login(&context, request, &response);
	if (!status.ok()) {
		LOG_ERROR("gRPC GetChatServer failed: {} ", status.error_message());
		response.set_error(static_cast<int>(ErrorCodes::RPC_FAILED));
	}
	else {
		LOG_DEBUG("gRPC Login succeeded for uid={}", uid);
	}
	_pool->ReturnConnection(std::move(stub));
	return response;
}

StatusGrpcClient::StatusGrpcClient()
{
	auto& configManager = ConfigManager::GetInstance();
	std::string host = configManager["StatusServer"]["host"];
	std::string port = configManager["StatusServer"]["port"];

	LOG_INFO("Initializing with host: {}, port: {}", host, port);

	_pool.reset(new StatusConPool((size_t)(std::thread::hardware_concurrency()), host, port));
}
