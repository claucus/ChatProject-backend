#include "ChatGrpcClient.h"
#include "Logger.h"

chat::KickUserResp ChatGrpcClient::KickUser(const std::string& host, int port, const std::string& uid, int reason)
{
	grpc::ClientContext ctx;
	chat::KickUserReq req;
	chat::KickUserResp resp;

	req.set_uid(uid);
	req.set_reason(reason);

	auto pool = initPool(host, port);
	auto stub = pool->GetConnection();

	auto status = stub->KickUser(&ctx, req, &resp);
	pool->ReturnConnection(std::move(stub));

	if (!status.ok()) {
		LOG_ERROR("ChatGrpcClient KickUser RPC failed to {}:{}, error msg is {}", host, port, status.error_message());
		resp.set_error(1);
	}
	else {
		LOG_INFO("ChatGrpcClient KickUser success to {}:{} for uid={}", host, port, uid);
	}
	return resp;
}

ChatGrpcClient::ChatGrpcClient()
{

}

ChatGrpcClient::ChatPool* ChatGrpcClient::initPool(const std::string& host, int port)
{
	LOG_INFO("init host:{} ,port:{}", host, port);

	std::string key = host + ":" + std::to_string(port);
	{
		std::lock_guard<std::mutex> g(_poolMutex);
		auto it = _pools.find(key);
		if (it != _pools.end()) {
			return it->second.get();
		}
	}

	auto newPool = std::make_unique<ChatPool>(std::thread::hardware_concurrency(), host, std::to_string(port));

	ChatPool* res = newPool.get();
	{
		std::lock_guard<std::mutex> g(_poolMutex);
		_pools[key] = std::move(newPool);
	}
	return res;
}
