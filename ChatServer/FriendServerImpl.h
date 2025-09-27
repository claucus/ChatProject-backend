#pragma once

#include <grpcpp/grpcpp.h>
#include "message.grpc.pb.h"
#include "UserInfo.h"

class FriendServerImpl final :public message::FriendService::Service
{
public:
	FriendServerImpl();

	grpc::Status SendFriend(grpc::ServerContext* context, const message::FriendRequest* request, message::FriendResponse* response) override;
	grpc::Status HandleFriend(grpc::ServerContext* context, const message::FriendApprovalRequest* request, message::FriendApprovalResponse* response) override;

private:
	bool GetUserInfo(const std::string& baseKey,const std::string& uid,std::shared_ptr<UserInfo>& userInfo);
};

