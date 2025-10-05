#include "FriendGrpcClient.h"
#include "ConfigManager.h"
#include "Defer.h"
#include "Logger.h"
#include "const.h"

message::FriendResponse FriendGrpcClient::SendFriend(const std::string server_ip, const message::FriendRequest& request)
{
	grpc::ClientContext context;
	message::FriendResponse response;

	auto applicant = request.applicant();
	auto recipient = request.recipient();

	LOG_INFO("applicant: {} ,and recipient: {}", applicant, recipient);

	defer{
		response.set_applicant(applicant);
		response.set_recipient(recipient);
		response.set_error(static_cast<int>(ErrorCodes::SUCCESS));
	};

	auto iter = _pools.find(server_ip);

	if (iter == _pools.end()) {
		return response;
	}

	auto& pool = iter->second;
	auto stub = pool->GetConnection();
	LOG_INFO("Calling SendFriend from applicant: {} to recipient: {} on {}", applicant, recipient, server_ip);
	auto status = stub->SendFriend(&context, request, &response);

	if (!status.ok()) {
		response.set_error(static_cast<int>(ErrorCodes::RPC_FAILED));
		LOG_ERROR("gRPC SendFriend Failed:{}", status.error_message());
		return response;
	}
	LOG_DEBUG("gRPC FriendGrpcClient succeeded from applicant: {} to recipient: {} on {}", applicant, recipient);
	pool->ReturnConnection(std::move(stub));

	return response;
}

message::FriendApprovalResponse FriendGrpcClient::HandleFriend(const std::string server_ip, const message::FriendApprovalRequest& request)
{
	grpc::ClientContext context;
	message::FriendApprovalResponse response;

	auto applicant = request.applicant();
	auto recipient = request.recipient();

	LOG_INFO("applicant: {} ,and recipient: {}", applicant, recipient);

	defer{
		response.set_applicant(applicant);
		response.set_recipient(recipient);
		response.set_error(static_cast<int>(ErrorCodes::SUCCESS));
	};

	auto iter = _pools.find(server_ip);
	
	if (iter == _pools.end()) {
		return response;
	}

	auto& pool = iter->second;
	auto stub = pool->GetConnection();
	LOG_INFO("Calling HandleFriend from applicant: {} to recipient: {} on {}", applicant, recipient, server_ip);
	auto status = stub->HandleFriend(&context, request, &response);

	if (!status.ok()) {
		response.set_error(static_cast<int>(ErrorCodes::RPC_FAILED));
		LOG_ERROR("gRPC HandleFriend Failed:{}", status.error_message());
		return response;
	}

	LOG_DEBUG("gRPC HandleFriend succeeded from applicant: {} to recipient: {} on {}", applicant, recipient);
	pool->ReturnConnection(std::move(stub));


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