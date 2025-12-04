#include "VerifyGrpcClient.h"
#include "ConfigManager.h"
#include "Logger.h"

verify::GetVerifyResp VerifyGrpcClient::GetVerifyCode(std::string email)
{
	grpc::ClientContext context;
	verify::GetVerifyReq request;
	verify::GetVerifyResp response;
	
	request.set_email(email);
	auto stub = _pool->GetConnection();
	LOG_INFO("Calling GetVerifyCode for email={}", email);
	auto status = stub->GetVerifyCode(&context, request, &response);

	if (!status.ok()) {
		LOG_ERROR("gRPC GetVerifyCode failed: {} ", status.error_message());
		response.set_error(static_cast<int>(ErrorCodes::RPC_FAILED));
	}
	else {
		LOG_DEBUG("gRPC GetVerifyCode succeeded for email={}", email);
	}
	_pool->ReturnConnection(std::move(stub));
	return response;
}

VerifyGrpcClient::VerifyGrpcClient()
{
	auto& configManager = ConfigManager::GetInstance();
	std::string host = configManager["VerifyServer"]["host"];
	std::string port = configManager["VerifyServer"]["port"];

	LOG_INFO("Initializing with host: {}, port: {}", host, port);

	_pool.reset(new VerifyConPool((size_t)(std::thread::hardware_concurrency()), host, port));
}
