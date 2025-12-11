#include "StatusGrpcClient.h"
#include "ConfigManager.h"
#include "Logger.h"

StatusGrpcClient::StatusGrpcClient() 
{
	auto& cfg = ConfigManager::GetInstance();
	std::string host = cfg["StatusServer"]["host"];
	std::string port = cfg["StatusServer"]["port"];
	LOG_INFO("StatusGrpcClient init {}:{}", host, port);
	_pool.reset(new StatusPool(std::thread::hardware_concurrency(), host, port));
}

status::AllocateServerResp StatusGrpcClient::AllocateServer(const std::string& uid) 
{
	grpc::ClientContext ctx;
	status::AllocateServerReq req;
	status::AllocateServerResp resp;

	req.set_uid(uid);
	
	auto stub = _pool->GetConnection();
	auto status = stub->AllocateServer(&ctx, req, &resp);
	_pool->ReturnConnection(std::move(stub));
	if (!status.ok()) {
		LOG_ERROR("AllocateServer RPC failed: {}", status.error_message());
		resp.set_error(1);
	}
	return resp;
}