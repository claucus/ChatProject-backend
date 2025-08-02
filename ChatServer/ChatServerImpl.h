#pragma once
#include <grpcpp/grpcpp.h>
#include "message.grpc.pb.h"

class ChatServerImpl final: public message::ChatService::Service
{
public:
	ChatServerImpl();

	grpc::Status SendMessage(
		grpc::ServerContext* context, 
		const message::ChatMessage* request, 
		message::SendMessageResponse* response) override;

	grpc::Status AddFriend(
		grpc::ServerContext* context, 
		const message::AddFriendRequest* request, 
		message::AddFriendResponse* response) override;

	grpc::Status NotifyAddFriend(
		grpc::ServerContext* context, 
		const message::NotifyAddFriendRequest* request, 
		message::NotifyAddFriendResponse* response) override;

	grpc::Status GetFriendList(
		grpc::ServerContext* context,
		const message::GetFriendListRequest* request,
		message::GetFriendListResponse* response) override;

private:

};

