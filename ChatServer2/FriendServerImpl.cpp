#include "FriendServerImpl.h"
#include "UserManager.h"
#include "CSession.h"
#include "const.h"
#include "Logger.h"
#include "Defer.h"
#include "RedisConPool.h"
#include "MySQLManager.h"

#include <nlohmann/json.hpp>
using json = nlohmann::json;

FriendServerImpl::FriendServerImpl()
{

}

grpc::Status FriendServerImpl::SendFriend(grpc::ServerContext* context, const message::FriendRequest* request, message::FriendResponse* response)
{
	auto applicant = request->applicant();
	auto recipient = request->recipient();

	LOG_INFO("Received friend request from {} to {}", applicant, recipient);
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
	notify["username"] = request->username();

	session->Send(notify.dump(4), static_cast<int>(MessageID::MESSAGE_NOTIFY_ADD_FRIEND));

	return grpc::Status::OK;
}

grpc::Status FriendServerImpl::HandleFriend(grpc::ServerContext* context, const message::FriendApprovalRequest* request, message::FriendApprovalResponse* response)
{
	auto applicant = request->applicant();
	auto recipient = request->recipient();

	LOG_INFO("Received friend approval from {} to {}", applicant, recipient);
	auto session = UserManager::GetInstance()->GetSession(request->recipient());

	defer{
		response->set_error(static_cast<int>(ErrorCodes::SUCCESS));
		response->set_applicant(applicant);
		response->set_recipient(recipient);
	};

	if (session == nullptr) {
		return grpc::Status::OK;
	}

	auto userInfo = std::make_shared<UserInfo>();
	std::string baseKey = ChatServiceConstant::USER_INFO_PREFIX + request->applicant();


	json notify;
	auto baseInfoExists = GetUserInfo(baseKey,request->applicant(),userInfo);

	if (!baseInfoExists) {
		notify["error"] = static_cast<int>(ErrorCodes::UID_INVALID);
	}
	else {
		notify["error"] = static_cast<int>(ErrorCodes::SUCCESS);
		notify["uid"] = request->applicant();
		notify["username"] = userInfo->_username;
		notify["avatar"] = userInfo->_avatar;
		notify["email"] = userInfo->_email;
		notify["birth"] = userInfo->_birth;
		notify["sex"] = userInfo->_sex;

		notify["grouping"] = request->grouping();
		notify["remark"] = request->remark();
	}
	
	session->Send(notify.dump(4),static_cast<int>(MessageID::MESSAGE_NOTIFY_APPROVAL_FRIEND));

	return grpc::Status::OK;
}

bool FriendServerImpl::GetUserInfo(const std::string& baseKey, const std::string& uid, std::shared_ptr<UserInfo>& userInfo)
{
	auto baseInfo = RedisConPool::GetInstance().get(baseKey).value();

	if (!baseInfo.empty()) {
		try {
			auto src = json::parse(baseInfo);
			userInfo->_uid = src["uid"].get<std::string>();
			userInfo->_username = src["username"].get<std::string>();
			userInfo->_password = src["password"].get<std::string>();
			userInfo->_avatar = src["avatar"].get<std::string>();
			userInfo->_birth = src["birth"].get<std::string>();
			userInfo->_sex = src["sex"].get<std::string>();
			userInfo->_email = src["email"].get<std::string>();

			LOG_INFO("Retrieved user info for uid: {}", uid);
		}
		catch (const json::parse_error& e) {
			LOG_WARN("Failed to parse JSON in GetUserInfo: {}", e.what());
			return false;
		}
		return true;
	}
	else {
		LOG_ERROR("No user info found for uid: {}", uid);

		std::shared_ptr<UserInfo> tmp_userInfo = nullptr;
		tmp_userInfo = MySQLManager::GetInstance()->GetUser(uid);

		if (tmp_userInfo == nullptr) {
			LOG_ERROR("No user found in MySQL for uid: {}", uid);
			return false;
		}

		userInfo = tmp_userInfo;

		json root;
		root["uid"] = userInfo->_uid;
		root["username"] = userInfo->_username;
		root["email"] = userInfo->_email;
		root["password"] = userInfo->_password;
		root["birth"] = userInfo->_birth;
		root["avatar"] = userInfo->_avatar;
		root["sex"] = userInfo->_sex;

		std::string redisString = root.dump(4);
		RedisConPool::GetInstance().set(baseKey, redisString);
		LOG_INFO("Cached user info for uid: {}", uid);
	}

	return true;
}
