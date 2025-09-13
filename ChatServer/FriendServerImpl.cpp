#include "FriendServerImpl.h"
#include "UserManager.h"
#include "CSession.h"
#include "const.h"
#include "Logger.h"
#include "Defer.h"

#include <nlohmann/json.hpp>
using json = nlohmann::json;

FriendServerImpl::FriendServerImpl()
{

}

grpc::Status FriendServerImpl::SendFriend(grpc::ServerContext* context, const message::FriendRequest* request, message::FriendResponse* response)
{
	auto applicant = request->applicant();
	auto recipient = request->recipient();

	LOG_INFO("Recived friend request from {} to {}", applicant, recipient);
	auto session = UserManager::GetInstance()->GetSession(request->recipient());


	defer{
		response->set_error(static_cast<int>(ErrorCodes::SUCCESS));
		response->set_applicant(applicant);
		response->set_recipient(recipient);
	};
	if (session == nullptr) {
		return grpc::Status::OK;
	}

	json notify;
	notify["error"] = static_cast<int>(ErrorCodes::SUCCESS);
	notify["uid"] = request->applicant();
	notify["avatar"] = request->avatar();
	notify["comments"] = request->message();
	notify["time"] = request->time();

	session->Send(notify.dump(4), static_cast<int>(MessageID::MESSAGE_NOTIFY_ADD_FRIEND));

	return grpc::Status::OK;
}

grpc::Status FriendServerImpl::HandleFriend(grpc::ServerContext* context, const message::FriendApprovalRequest* request, message::FriendApprovalResponse* response)
{
	return grpc::Status::OK;
}
