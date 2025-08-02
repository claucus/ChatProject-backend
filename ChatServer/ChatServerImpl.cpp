#include "ChatServerImpl.h"

ChatServerImpl::ChatServerImpl()
{

}

grpc::Status ChatServerImpl::SendMessage(grpc::ServerContext* context, const message::ChatMessage* request, message::SendMessageResponse* response)
{
	return grpc::Status::OK;
}

grpc::Status ChatServerImpl::AddFriend(grpc::ServerContext* context, const message::AddFriendRequest* request, message::AddFriendResponse* response)
{
	return grpc::Status::OK;
}

grpc::Status ChatServerImpl::NotifyAddFriend(grpc::ServerContext* context, const message::NotifyAddFriendRequest* request, message::NotifyAddFriendResponse* response)
{
	return grpc::Status::OK;
}

grpc::Status ChatServerImpl::GetFriendList(grpc::ServerContext* context, const message::GetFriendListRequest* request, message::GetFriendListResponse* response)
{
	return grpc::Status::OK;
}
