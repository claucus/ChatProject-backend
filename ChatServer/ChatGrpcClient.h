#pragma once

#include <unordered_map>
#include <grpcpp/grpcpp.h>
#include "message.grpc.pb.h"
#include "Singleton.h"
#include "GrpcPool.h"
#include "const.h"

class ChatGrpcClient: public Singleton<ChatGrpcClient>
{
	using ChatConPool = GrpcPool<message::ChatService, message::ChatService::Stub>;
	friend class Singleton<ChatGrpcClient>;

public:
	message::SendMessageResponse SendMessage(const message::ChatMessage& request);
	message::AddFriendResponse AddFriend(const message::AddFriendRequest& request);
	message::NotifyAddFriendResponse NotifyAddFriend(const message::NotifyAddFriendRequest& request);
	message::GetFriendListResponse GetFriendList(const message::GetChatServerRequest& request);

private:
	ChatGrpcClient();
	std::unordered_map <std::string, std::unique_ptr<ChatConPool> > _pools;
};

