#include "FriendGrpcClient.h"
#include "ConfigManager.h"
#include "Defer.h"
#include "Logger.h"
#include "const.h"

message::FriendResponse FriendGrpcClient::SendFriend(const std::string server_ip, const message::FriendRequest& request)
{
	grpc::ClientContext context;
	message::FriendResponse response;

	defer{
		response.set_applicant(request.applicant());
		response.set_recipient(request.recipient());
		response.set_error(static_cast<int>(ErrorCodes::SUCCESS));
	};

	auto iter = _pools.find(server_ip);

	if (iter == _pools.end()) {
		return response;
	}

	auto& pool = iter->second;
	auto stub = pool->GetConnection();
	LOG_INFO("Calling SendFriend from applicant: {} to recipient: {} on {}", request.applicant(), request.recipient(), server_ip);
	auto status = stub->SendFriend(&context, request, &response);

	if (!status.ok()) {
		response.set_error(static_cast<int>(ErrorCodes::RPC_FAILED));
		LOG_ERROR("gRPC SendFriend Failed:{}", status.error_message());
		return response;
	}
	LOG_DEBUG("gRPC FriendGrpcClient succeeded from applicant: {} to recipient: {} on {}", request.applicant(), request.recipient());
	pool->ReturnConnection(std::move(stub));

	return response;
}

message::FriendApprovalResponse FriendGrpcClient::HandleFriend(const message::FriendApprovalRequest& request)
{
	message::FriendApprovalResponse response;
	return response;
}

FriendGrpcClient::FriendGrpcClient() 
{
	auto& cfg = ConfigManager::GetInstance();
	
	auto serverList = cfg["PeerServer"]["ServerList"];

	std::vector<std::string> servers;
	std::stringstream ss(serverList);
	std::string serverName;

	while (std::getline(ss, serverName, ',')) {
		servers.emplace_back(serverName);
	}

	for (auto& serverName : servers) {
		if (cfg[serverName]["name"].empty()) {
			continue;
		}
		_pools[cfg[serverName]["name"]] =
			std::make_unique<ChatConPool>(
				(size_t)(std::thread::hardware_concurrency()),
				cfg[serverName]["host"],
				cfg[serverName]["port"]
			);
	}
}