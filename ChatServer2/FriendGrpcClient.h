#pragma once

#include <unordered_map>
#include <grpcpp/grpcpp.h>
#include "message.grpc.pb.h"
#include "Singleton.h"
#include "GrpcPool.h"
#include "const.h"

class FriendGrpcClient:public Singleton<FriendGrpcClient>
{
	using ChatConPool = GrpcPool<message::FriendService, message::FriendService::Stub>;
	friend class Singleton<FriendGrpcClient>;

public:
	message::FriendResponse SendFriend(const std::string server_ip, const message::FriendRequest& request);
	message::FriendApprovalResponse HandleFriend(const std::string server_ip, const message::FriendApprovalRequest& request);


private:
	FriendGrpcClient();
	std::unordered_map<std::string, std::unique_ptr<ChatConPool>> _pools;
};

