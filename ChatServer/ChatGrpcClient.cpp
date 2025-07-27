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
	//auto& configManager = ConfigManager::GetInstance();
	//std::string host = configManager["text"]["host"];
	//std::string port = configManager["text"]["port"];

}
