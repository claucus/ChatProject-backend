#include "ChatGrpcClient.h"
#include "ConfigManager.h"

message::SendMessageResponse ChatGrpcClient::SendMessage(const message::ChatMessage& request)
{
	grpc::ClientContext context;
	message::SendMessageResponse response;

	/*auto stub = _pools*/
	return response;
}

message::AddFriendResponse ChatGrpcClient::AddFriend(const message::AddFriendRequest& request)
{
	message::AddFriendResponse response;
	

	return response;
}

message::NotifyAddFriendResponse ChatGrpcClient::NotifyAddFriend(const message::NotifyAddFriendRequest& request)
{
	message::NotifyAddFriendResponse response;
	
	return response;
}

message::GetFriendListResponse ChatGrpcClient::GetFriendList(const message::GetChatServerRequest& request)
{
	message::GetFriendListResponse response;
	
	return response;
}

ChatGrpcClient::ChatGrpcClient()
{
	auto& configManager = ConfigManager::GetInstance();
	
	auto serverList = configManager["PeerServer"]["ServerList"];

	std::vector<std::string> servers;

	std::stringstream ss(serverList);
	std::string serverName;
	
	while (std::getline(ss, serverName, ',')) {
		servers.emplace_back(serverName);
	}


	for (auto& serverName : servers) {
		if (configManager[serverName]["name"].empty()) {
			continue;
		}
		_pools[configManager[serverName]["name"]] = 
			std::make_unique<ChatConPool>(
				(size_t)(std::thread::hardware_concurrency()),
				configManager[serverName]["host"],
				configManager[serverName]["port"]
			);
	}
}
