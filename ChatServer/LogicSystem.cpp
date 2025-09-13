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
    LOG_INFO("[LogicSystem] Destructor called, stopping message processing...");
    _b_stop = true;
    _consume.notify_one();
    if (_thread.joinable()) {
        _thread.join();
        LOG_INFO("[LogicSystem] Worker thread joined successfully");
    }
}

void LogicSystem::PostMessageToQueue(std::shared_ptr<LogicNode> message)
{
    std::unique_lock<std::mutex> lock(_mutex);
    _messageQueue.push(message);
    LOG_DEBUG("[LogicSystem] Message queued, queue size: {}", _messageQueue.size());

    if (_messageQueue.size() == 1) {
        lock.unlock();
        _consume.notify_one();
        LOG_DEBUG("[LogicSystem] Consumer thread notified");
    }
}

LogicSystem::LogicSystem():
    _b_stop(false)
{
    LOG_INFO("[LogicSystem] Initializing...");
    RegisterCallBack();
    _thread = std::thread(&LogicSystem::DealMessage, this);
    LOG_INFO("[LogicSystem] Worker thread started");
}

void LogicSystem::DealMessage()
{
    LOG_INFO("[LogicSystem] Message processing thread started");
    while (true) {
        std::unique_lock<std::mutex> lock(_mutex);
        while (_messageQueue.empty() && !_b_stop) {
            _consume.wait(lock);
        }

        if (_b_stop) {
            LOG_INFO("[LogicSystem] Stopping message processing, remaining messages: {}", _messageQueue.size());
            while (!_messageQueue.empty()) {
                auto messageNode = _messageQueue.front();

                auto callBackIter = _funcCallBack.find(messageNode->_receiveNode->GetId());
                if (callBackIter == _funcCallBack.end()) {
                    LOG_WARN("[LogicSystem] No callback found for message ID: {}", messageNode->_receiveNode->GetId());
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
        LOG_DEBUG("[LogicSystem] Processing message ID: {}", messageNode->_receiveNode->GetId());

        auto callBackIter = _funcCallBack.find(messageNode->_receiveNode->GetId());
        if (callBackIter == _funcCallBack.end()) {
            LOG_WARN("[LogicSystem] No handler registered for message ID: {}", messageNode->_receiveNode->GetId());
            _messageQueue.pop();
            continue;
        }
        callBackIter->second(
            messageNode->_session,
            messageNode->_receiveNode->GetId(),
            std::string(messageNode->_receiveNode->_data, messageNode->_receiveNode->_currentLength)
        );
        _messageQueue.pop();
        LOG_DEBUG("[LogicSystem] Message processed, remaining queue size: {}", _messageQueue.size());
    }
}

void LogicSystem::RegisterCallBack()
{
    LOG_INFO("[LogicSystem] Registering message callbacks...");
    auto id = static_cast<size_t>(MessageID::MESSAGE_CHAT_LOGIN);
    _funcCallBack[id] = std::bind(&LogicSystem::LoginHandler,this,
            std::placeholders::_1,
            std::placeholders::_2,
            std::placeholders::_3);
    LOG_INFO("[LogicSystem] Registered login handler for message ID: {}", id);


    id = static_cast<size_t>(MessageID::MESSAGE_GET_SEARCH_USER);
    _funcCallBack[id] = std::bind(&LogicSystem::SearchHandler, this,
            std::placeholders::_1,
            std::placeholders::_2,
            std::placeholders::_3);
    LOG_INFO("[LogicSystem] Registered search handler for message ID: {}", id);


    id = static_cast<size_t>(MessageID::MESSAGE_APPLY_FRIEND);
    _funcCallBack[id] = std::bind(&LogicSystem::ApplyFriendHandler,this,
        std::placeholders::_1,
        std::placeholders::_2,
        std::placeholders::_3);
    LOG_INFO("[LogicSystem] Registered apply friend handler for message ID: {}", id);
}

void LogicSystem::LoginHandler(std::shared_ptr<CSession> session, const size_t& messageId, const std::string& messageData)
{
    LOG_INFO("[LogicSystem] Processing login request...");

    json root;
    defer{
		std::string returnStr = root.dump(4);
		session->Send(returnStr, static_cast<size_t>(MessageID::MESSAGE_CHAT_LOGIN_RESPONSE));
    };

    try {
        auto src = json::parse(messageData);

        std::string uid = src["uid"].get<std::string>();
        std::string token = src["token"].get<std::string>();
        LOG_INFO("[LogicSystem] Login attempt - UID: {}, Token length: {}", uid, token.length());


        std::string tokenKey = ChatServiceConstant::USER_TOKEN_PREFIX + uid;
        auto tokenValue = RedisConPool::GetInstance().get(tokenKey).value();

		if (tokenValue.empty()) {
			LOG_ERROR("[LogicSystem] Token not found in Redis for UID: {}", uid);
			root["error"] = static_cast<size_t>(ErrorCodes::UID_INVALID);
			return;
		}

		if (tokenValue != token) {
			LOG_ERROR("[LogicSystem] Token mismatch for UID: {}", uid);
			root["error"] = static_cast<size_t>(ErrorCodes::TOKEN_INVALID);
			return;
		}


		root["error"] = static_cast<size_t>(ErrorCodes::SUCCESS);
		std::string baseKey = ChatServiceConstant::USER_INFO_PREFIX + uid;
        auto userInfo = std::make_shared<UserInfo>();
        
        auto baseInfoExists = GetUserInfo(baseKey, uid, userInfo);

		if (!baseInfoExists) {
			LOG_ERROR("[LogicSystem] User info not found for UID: {}", uid);
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


		auto serverName = ConfigManager::GetInstance().getValue("SelfServer", "name");

		auto redisResult = RedisConPool::GetInstance().hget(ChatServiceConstant::LOGIN_COUNT, serverName).value();
		int loginCount = 0;

		if (!redisResult.empty()) {
			loginCount = std::stoi(redisResult);
		}

		loginCount++;

		RedisConPool::GetInstance().hset(ChatServiceConstant::LOGIN_COUNT, serverName, std::to_string(loginCount));

		session->SetUid(uid);

		std::string ipKey = ChatServiceConstant::USER_IP_PREFIX + uid;
		RedisConPool::GetInstance().set(ipKey, serverName);

		UserManager::GetInstance()->setUserSession(uid, session);

		LOG_INFO("[LogicSystem] Login successful for UID: {}", uid);

		return;
    }
    catch (const json::parse_error& e) {
        LOG_WARN("[LogicSystem] Failed to parse JSON in LoginHandler: {}", e.what());
        root["error"] = static_cast<int>(ErrorCodes::ERROR_JSON);
    }
}

void LogicSystem::SearchHandler(std::shared_ptr<CSession> session, const size_t& messageId, const std::string& messageData)
{
    LOG_INFO("[LogicSystem] Processing search request...");

    json root;
    defer{
        std::string returnStr = root.dump(4);
        session->Send(returnStr, static_cast<size_t>(MessageID::MESSAGE_GET_SEARCH_USER_RESPONSE));
    };

    try {
        auto src = json::parse(messageData);

        std::string uid = src["uid"].get<std::string>();
        std::string selfUid = src["self"].get<std::string>();

        LOG_INFO("[LogicSystem] Search attempt - UID: {}", uid);

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
        LOG_WARN("[LogicSystem] Failed to parse JSON in SearchHandler: {}", e.what());
        root["error"] = static_cast<int>(ErrorCodes::ERROR_JSON);
    }
}

void LogicSystem::ApplyFriendHandler(std::shared_ptr<CSession> session, const size_t& messageId, const std::string& messageData)
{
	LOG_INFO("[LogicSystem] Processing Apply Friend request...");

	json root;
	defer{
		std::string returnStr = root.dump(4);
		session->Send(returnStr, static_cast<size_t>(MessageID::MESSAGE_APPLY_FRIEND_RESPONSE));
	};

    try {
        auto src = json::parse(messageData);

        std::string to_uid = src["uid"].get<std::string>();
        std::string from_uid = src["self"].get<std::string>();
        std::string group_other = src["grouping"].get<std::string>();
        std::string comments = src["comments"].get<std::string>();
        auto now_time = std::chrono::system_clock::now().time_since_epoch();


        LOG_INFO("[LogicSystem] Friend attempt - UID: {}", from_uid);

        root["error"] = static_cast<int>(ErrorCodes::SUCCESS);

        auto relation = FriendRelation(from_uid, to_uid, static_cast<int>(AddStatusCodes::NotConsent), group_other);

        auto success = MySQLManager::GetInstance()->AddFriend(relation);
        if (!success) {
            return;
        }


        std::string ipKey = ChatServiceConstant::USER_IP_PREFIX + to_uid;
        auto to_ip_value = RedisConPool::GetInstance().get(ipKey).value();
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
        }

        FriendGrpcClient::GetInstance()->SendFriend(to_ip_value,request);
	}
	catch (const json::parse_error& e) {
		LOG_WARN("[LogicSystem] Failed to parse JSON in ApplyFriendHandler: {}", e.what());
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
			LOG_INFO("[LogicSystem] Retrieved user info for uid: {}", uid);
        }
        catch(const json::parse_error& e){
			LOG_WARN("[LogicSystem] Failed to parse JSON in GetUserInfo: {}", e.what());
        }
        return true;
	}
	else {
		LOG_ERROR("[LogicSystem] No user info found for uid: {}", uid);

		std::shared_ptr<UserInfo> tmp_userInfo = nullptr;
		tmp_userInfo = MySQLManager::GetInstance()->GetUser(uid);

		if (tmp_userInfo == nullptr) {
			LOG_ERROR("[LogicSystem] No user found in MySQL for uid: {}", uid);
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
		LOG_INFO("[LogicSystem] Cached user info for uid: {}", uid);
	}

	return true;
}
