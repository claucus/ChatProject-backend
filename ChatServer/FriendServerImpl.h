#pragma once

#include <grpcpp/grpcpp.h>
#include "message.grpc.pb.h"

class FriendServerImpl final :public message::FriendService::Service
{
public:
	FriendServerImpl();

	grpc::Status SendFriend(grpc::ServerContext* context, const message::FriendRequest* request, message::FriendResponse* response) override;
	grpc::Status HandleFriend(grpc::ServerContext* context, const message::FriendApprovalRequest* request, message::FriendApprovalResponse* response) override;

private:

};

