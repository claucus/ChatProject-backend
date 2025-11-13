#include "LogicSystem.h"
#include "StatusGrpcClient.h"
#include "MySQLManager.h"
#include "RedisConPool.h"
#include "ConfigManager.h"
#include "UserManager.h"
#include "Defer.h"
#include "Logger.h"

#include <chrono>
#include "FriendGrpcClient.h"

LogicSystem::~LogicSystem()
{
    LOG_INFO("Destructor called, stopping message processing...");
    _b_stop = true;
    _consume.notify_one();
    if (_thread.joinable()) {
        _thread.join();
        LOG_INFO("Worker thread joined successfully");
    }
}

void LogicSystem::PostMessageToQueue(std::shared_ptr<LogicNode> message)
{
    std::unique_lock<std::mutex> lock(_mutex);
    _messageQueue.push(message);
    LOG_DEBUG("Message queued, queue size: {}", _messageQueue.size());

    if (_messageQueue.size() == 1) {
        lock.unlock();
        _consume.notify_one();
        LOG_DEBUG("Consumer thread notified");
    }
}

LogicSystem::LogicSystem():
    _b_stop(false)
{
    LOG_INFO("Initializing...");
    RegisterCallBack();
    _thread = std::thread(&LogicSystem::DealMessage, this);
    LOG_INFO("Worker thread started");
}

void LogicSystem::DealMessage()
{
    LOG_INFO("Message processing thread started");
    while (true) {
        std::unique_lock<std::mutex> lock(_mutex);
        while (_messageQueue.empty() && !_b_stop) {
            _consume.wait(lock);
        }

        if (_b_stop) {
            LOG_INFO("Stopping message processing, remaining messages: {}", _messageQueue.size());
            while (!_messageQueue.empty()) {
                auto messageNode = _messageQueue.front();

                auto callBackIter = _funcCallBack.find(messageNode->_receiveNode->GetId());
                if (callBackIter == _funcCallBack.end()) {
                    LOG_WARN("No callback found for message ID: {}", messageNode->_receiveNode->GetId());
                    _messageQueue.pop();
                    continue;
                }

                callBackIter->second(
                    messageNode->_session,
                    messageNode->_receiveNode->GetId(),
                    std::string(messageNode->_receiveNode->_data,messageNode->_receiveNode->_currentLength)
                );
                _messageQueue.pop();
            }
            break;
        }

        auto messageNode = _messageQueue.front();
        LOG_DEBUG("Processing message ID: {}", messageNode->_receiveNode->GetId());

        auto callBackIter = _funcCallBack.find(messageNode->_receiveNode->GetId());
        if (callBackIter == _funcCallBack.end()) {
            LOG_WARN("No handler registered for message ID: {}", messageNode->_receiveNode->GetId());
            _messageQueue.pop();
            continue;
        }
        callBackIter->second(
            messageNode->_session,
            messageNode->_receiveNode->GetId(),
            std::string(messageNode->_receiveNode->_data, messageNode->_receiveNode->_currentLength)
        );
        _messageQueue.pop();
        LOG_DEBUG("Message processed, remaining queue size: {}", _messageQueue.size());
    }
}

void LogicSystem::RegisterCallBack()
{
    LOG_INFO("Registering message callbacks...");
    auto id = static_cast<size_t>(MessageID::MESSAGE_CHAT_LOGIN);
    _funcCallBack[id] = std::bind(&LogicSystem::LoginHandler,this,
            std::placeholders::_1,
            std::placeholders::_2,
            std::placeholders::_3);
    LOG_INFO("Registered login handler for message ID: {}", id);


    id = static_cast<size_t>(MessageID::MESSAGE_GET_SEARCH_USER);
    _funcCallBack[id] = std::bind(&LogicSystem::SearchHandler, this,
            std::placeholders::_1,
            std::placeholders::_2,
            std::placeholders::_3);
    LOG_INFO("Registered search handler for message ID: {}", id);


    id = static_cast<size_t>(MessageID::MESSAGE_APPLY_FRIEND);
    _funcCallBack[id] = std::bind(&LogicSystem::ApplyFriendHandler,this,
        std::placeholders::_1,
        std::placeholders::_2,
        std::placeholders::_3);
    LOG_INFO("Registered apply friend handler for message ID: {}", id);

    
    id = static_cast<size_t>(MessageID::MESSAGE_APPROVAL_FRIEND);
	_funcCallBack[id] = std::bind(&LogicSystem::ApprovalFriendHandler, this,
        std::placeholders::_1,
        std::placeholders::_2,
		std::placeholders::_3);
	LOG_INFO("Registered approval friend handler for message ID: {}", id);

}

void LogicSystem::LoginHandler(std::shared_ptr<CSession> session, const size_t& messageId, const std::string& messageData)
{
    LOG_INFO("Processing login request...");

    json root;
    defer{
		std::string returnStr = root.dump(4);
		session->Send(returnStr, static_cast<size_t>(MessageID::MESSAGE_CHAT_LOGIN_RESPONSE));
        LOG_INFO("Send json is {}", returnStr);
    };

    try {
        auto src = json::parse(messageData);

        std::string uid = src["uid"].get<std::string>();
        std::string token = src["token"].get<std::string>();
        LOG_INFO("Login attempt - UID: {}, Token length: {}", uid, token.length());

        std::string sessionOpt = RedisConPool::GetInstance().get(ChatServiceConstant::USER_SESSION_PREFIX + uid).value();
        auto ttlTime = RedisConPool::GetInstance().ttl(ChatServiceConstant::USER_SESSION_PREFIX + uid);
        auto sessionJson = json::parse(sessionOpt);

        std::string tokenValue = sessionJson["token"].get<std::string>();

		if (tokenValue.empty()) {
			LOG_ERROR("Token not found in Redis for UID: {}", uid);
			root["error"] = static_cast<size_t>(ErrorCodes::UID_INVALID);
			return;
		}

		if (tokenValue != token) {
			LOG_ERROR("Token mismatch for UID: {}", uid);
			root["error"] = static_cast<size_t>(ErrorCodes::TOKEN_INVALID);
			return;
		}


		root["error"] = static_cast<size_t>(ErrorCodes::SUCCESS);
		std::string baseKey = ChatServiceConstant::USER_INFO_PREFIX + uid;
        auto userInfo = std::make_shared<UserInfo>();
        
        auto baseInfoExists = GetUserInfo(baseKey, uid, userInfo);

		if (!baseInfoExists) {
			LOG_ERROR("User info not found for UID: {}", uid);
			root["error"] = static_cast<size_t>(ErrorCodes::UID_INVALID);
			return;
		}

        root["uid"] = uid;
        root["username"] = userInfo->_username;
        root["email"] = userInfo->_email;
        root["password"] = userInfo->_password;
		root["birth"] = userInfo->_birth;
		root["avatar"] = userInfo->_avatar;
		root["sex"] = userInfo->_sex;
        root["token"] = token;

        root["apply_list"] = json::array();
        
		auto applyList = MySQLManager::GetInstance()->GetApplyList(uid);
        if (!applyList.empty()) {
			for (const auto& apply : applyList) {
				json apply_json;
				apply_json["uid"] = apply->_uid;
				apply_json["username"] = apply->_username;

				apply_json["avatar"] = apply->_avatar;

				apply_json["comments"] = apply->_comments;
				apply_json["time"] = apply->_time;

				apply_json["add_status"] = apply->_status;

				root["apply_list"].push_back(apply_json);
			}
        }

        root["contact_friend_list"] = json::array();

        auto contactList = MySQLManager::GetInstance()->GetFriendList(uid);
        if (!contactList.empty()) {
            for (const auto& contact : contactList) {
                json contact_json;
                contact_json["uid"] = contact->_user->_uid;
                contact_json["username"] = contact->_user->_username;

                contact_json["avatar"] = contact->_user->_avatar;
				contact_json["email"] = contact->_user->_email;

				contact_json["birth"] = contact->_user->_birth;
                contact_json["sex"] = contact->_user->_sex;

				contact_json["group"] = contact->_group;
                contact_json["remark"] = contact->_remark;

				root["contact_friend_list"].push_back(contact_json);
            }
        }

		auto serverName = ConfigManager::GetInstance().getValue("SelfServer", "name");
        auto loginTimes = sessionJson["times"].get<std::string>();

        if (std::stoi(loginTimes) == 0) {
            sessionJson["times"] = "1";

            auto redisResult = RedisConPool::GetInstance().hget(ChatServiceConstant::LOGIN_COUNT, serverName).value();
			int loginCount = 0;

			if (!redisResult.empty()) {
				loginCount = std::stoi(redisResult);
			}

			loginCount++;
            RedisConPool::GetInstance().hset(ChatServiceConstant::LOGIN_COUNT, serverName, std::to_string(loginCount));
        }
        else {
            sessionJson["times"] = std::to_string(std::stoi(loginTimes) + 1);

        }
        RedisConPool::GetInstance().setex(ChatServiceConstant::USER_SESSION_PREFIX + uid, ttlTime, sessionJson.dump());

		session->SetUserUid(uid);

        std::string apply_list = ChatServiceConstant::FRIEND_REQUEST_PREFIX + uid + "_apply";
		std::string contact_list = ChatServiceConstant::FRIEND_REQUEST_PREFIX + uid + "_contact";

        RedisConPool::GetInstance().set(apply_list, root["apply_list"].dump(4));
		RedisConPool::GetInstance().set(contact_list, root["contact_friend_list"].dump(4));

		UserManager::GetInstance()->setUserSession(uid, session);

		LOG_INFO("Login successful for UID: {}", uid);

		return;
    }
    catch (const json::parse_error& e) {
        LOG_WARN("Failed to parse JSON in LoginHandler: {}", e.what());
        root["error"] = static_cast<int>(ErrorCodes::ERROR_JSON);
    }
}

void LogicSystem::SearchHandler(std::shared_ptr<CSession> session, const size_t& messageId, const std::string& messageData)
{
    LOG_INFO("Processing search request...");

    json root;
    defer{
        std::string returnStr = root.dump(4);
        session->Send(returnStr, static_cast<size_t>(MessageID::MESSAGE_GET_SEARCH_USER_RESPONSE));
        LOG_INFO("Send json is {}", returnStr);
    };

    try {
        auto src = json::parse(messageData);

        std::string uid = src["uid"].get<std::string>();
        std::string selfUid = src["self"].get<std::string>();

        LOG_INFO("Search attempt - UID: {}", uid);

        root["error"] = static_cast<int>(ErrorCodes::SUCCESS);
        root["users"] = json::array();

        auto users = MySQLManager::GetInstance()->FuzzySearchUsers(selfUid, uid);

        for (const auto& user : users) {
            json user_json;
            user_json["uid"] = user->_uid;
            user_json["username"] = user->_username;
            user_json["avatar"] = user->_avatar;
            user_json["add_status"] = user->_status;

            root["users"].push_back(user_json);

            std::string baseKey = ChatServiceConstant::USER_FRIEND_STATUS + user->_uid;
            RedisConPool::GetInstance().set(baseKey, user_json.dump(4));
        }

        if (root["users"].empty()) {
            root["error"] = static_cast<int>(ErrorCodes::UID_INVALID);
        }
       
    }
    catch (const json::parse_error& e) {
        LOG_WARN("Failed to parse JSON in SearchHandler: {}", e.what());
        root["error"] = static_cast<int>(ErrorCodes::ERROR_JSON);
    }
}

void LogicSystem::ApplyFriendHandler(std::shared_ptr<CSession> session, const size_t& messageId, const std::string& messageData)
{
	LOG_INFO("Processing Apply Friend request...");

	json root;
	defer{
		std::string returnStr = root.dump(4);
		session->Send(returnStr, static_cast<size_t>(MessageID::MESSAGE_APPLY_FRIEND_RESPONSE));
        LOG_INFO("Send json is {}", returnStr);
	};

    try {
        auto src = json::parse(messageData);
        LOG_INFO("parse json is {}", messageData);

        std::string to_uid = src["uid"].get<std::string>();
        std::string from_uid = src["self"].get<std::string>();
        std::string group_other = src["grouping"].get<std::string>();
        std::string comments = src["comments"].get<std::string>();
        std::string remark_other = src["remark"].get<std::string>();
        auto now_time = std::chrono::system_clock::now().time_since_epoch();


        LOG_INFO("Friend attempt - UID: {}", from_uid);
        
        root["uid"] = to_uid;
        root["error"] = static_cast<int>(ErrorCodes::SUCCESS);
        auto relation = FriendRelation(from_uid, to_uid, static_cast<int>(AddStatusCodes::NotConsent), group_other, remark_other);
        auto success = MySQLManager::GetInstance()->AddFriend(relation,comments);

        if (!success) {
            return;
        }

        std::string sessionOpt = RedisConPool::GetInstance().get(ChatServiceConstant::USER_SESSION_PREFIX + to_uid).value();
        auto sessionJson = json::parse(sessionOpt);

        auto to_ip_value = sessionJson["server_name"].get<std::string>();

        if (to_ip_value.empty()) {
            return;
        }


        auto& cfg = ConfigManager::GetInstance();
        auto selfServer = cfg["SelfServer"]["name"];

        std::string baseKey = ChatServiceConstant::USER_INFO_PREFIX + from_uid;
        auto userInfo = std::make_shared<UserInfo>();
        bool userFind = GetUserInfo(baseKey, from_uid, userInfo);

        if (to_ip_value == selfServer) {
            auto session = UserManager::GetInstance()->GetSession(to_uid);
            
            if (session) {
                json notify;
                notify["error"] = static_cast<int>(ErrorCodes::SUCCESS);
                notify["comments"] = comments;
                notify["uid"] = from_uid;
                notify["time"] = std::chrono::duration_cast<std::chrono::milliseconds>(now_time).count();
                
                if (userFind) {
                    notify["avatar"] = userInfo->_avatar;
                    notify["username"] = userInfo->_username;
                }

                session->Send(notify.dump(4), static_cast<int>(MessageID::MESSAGE_NOTIFY_ADD_FRIEND));
            }
            return;
        }

        message::FriendRequest request;
        request.set_applicant(from_uid);
        request.set_recipient(to_uid);
        request.set_message(comments);
        request.set_time(std::chrono::duration_cast<std::chrono::milliseconds>(now_time).count());
        if (userFind) {
            request.set_avatar(userInfo->_avatar);
            request.set_username(userInfo->_username);
        }

        FriendGrpcClient::GetInstance()->SendFriend(to_ip_value,request);
	}
	catch (const json::parse_error& e) {
		LOG_WARN("Failed to parse JSON in ApplyFriendHandler: {}", e.what());
        root["error"] = static_cast<int>(ErrorCodes::ERROR_JSON);
	}
}

void LogicSystem::ApprovalFriendHandler(std::shared_ptr<CSession> session, const size_t& messageId, const std::string& messageData)
{
	LOG_INFO("Processing Approval Friend request...");
    json root;

    defer{
        std::string returnStr = root.dump(4);
        session->Send(returnStr, static_cast<size_t>(MessageID::MESSAGE_APPROVAL_FRIEND_RESPONSE));
        LOG_INFO("Send json is {}", returnStr);
	};
    try {
        auto src = json::parse(messageData);
        LOG_INFO("parse json is {}", messageData);

        std::string to_uid = src["uid"].get<std::string>();
        std::string from_uid = src["self"].get<std::string>();
        std::string group_other = src["grouping"].get<std::string>();
        std::string remark_other = src["remark"].get<std::string>();

        LOG_INFO("Approval Friend attempt - UID: {}", from_uid);


		auto relation = FriendRelation(from_uid, to_uid, static_cast<int>(AddStatusCodes::MutualFriend), group_other, remark_other);
        auto success = MySQLManager::GetInstance()->UpdateFriendStatus(relation);

        if (!success) {
            return;
        }


        root["error"] = static_cast<int>(ErrorCodes::SUCCESS);
        std::string baseKey = ChatServiceConstant::USER_INFO_PREFIX + to_uid;
        auto userInfo = std::make_shared<UserInfo>();
        
		auto baseInfoExists = GetUserInfo(baseKey, to_uid, userInfo);

        if (!baseInfoExists) {
			LOG_ERROR("User info not found for UID: {}", to_uid);
            root["error"] = static_cast<int>(ErrorCodes::UID_INVALID);
			return;
        }

        root["uid"] = to_uid;
		root["username"] = userInfo->_username;
        root["email"] = userInfo->_email;
		root["birth"] = userInfo->_birth;
        root["avatar"] = userInfo->_avatar;
        root["sex"] = userInfo->_sex;
		root["grouping"] = group_other;
		root["remark"] = remark_other;


		auto sessionOpt = RedisConPool::GetInstance().get(ChatServiceConstant::USER_SESSION_PREFIX + to_uid).value();
		auto sessionJson = json::parse(sessionOpt);

		auto to_ip_value = sessionJson["server_name"].get<std::string>();

		if (to_ip_value.empty()) {
			return;
		}

        
        auto& cfg = ConfigManager::GetInstance();
        auto selfServer = cfg["SelfServer"]["name"];

        if (to_ip_value == selfServer) {
            auto session = UserManager::GetInstance()->GetSession(to_uid);
            if (session) {
                json notify;
                notify["uid"] = userInfo->_uid;
                notify["username"] = userInfo->_username;
				notify["email"] = userInfo->_email;
				notify["birth"] = userInfo->_birth;
                notify["avatar"] = userInfo->_avatar;
                notify["sex"] = userInfo->_sex;

                notify["grouping"] = group_other;
				notify["remark"] = remark_other;
                session->Send(notify.dump(4), static_cast<int>(MessageID::MESSAGE_NOTIFY_APPROVAL_FRIEND));
            }
            return;
        }

		message::FriendApprovalRequest request;
		request.set_applicant(from_uid);
		request.set_recipient(to_uid);
        request.set_grouping(group_other);
        request.set_remark(remark_other);

		FriendGrpcClient::GetInstance()->HandleFriend(to_ip_value, request);
    }
    catch (const json::parse_error& e) {
        LOG_WARN("Failed to parse JSON in ApprovalFriendHandler: {}", e.what());
        root["error"] = static_cast<int>(ErrorCodes::ERROR_JSON);
	}
}

bool LogicSystem::GetUserInfo(std::string baseKey, std::string uid, std::shared_ptr<UserInfo>& userInfo)
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
        catch(const json::parse_error& e){
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
