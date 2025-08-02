#include "LogicSystem.h"
#include "StatusGrpcClient.h"
#include "MySQLManager.h"
#include "RedisConPool.h"
#include "ConfigManager.h"
#include "UserManager.h"

#include <json/json.h>
#include <json/reader.h>
#include <json/value.h>

#include <spdlog/spdlog.h>

LogicSystem::~LogicSystem()
{
    spdlog::info("[LogicSystem] Destructor called, stopping message processing...");
    _b_stop = true;
    _consume.notify_one();
    if (_thread.joinable()) {
        _thread.join();
        spdlog::info("[LogicSystem] Worker thread joined successfully");
    }
}

void LogicSystem::PostMessageToQueue(std::shared_ptr<LogicNode> message)
{
    std::unique_lock<std::mutex> lock(_mutex);
    _messageQueue.push(message);
    spdlog::debug("[LogicSystem] Message queued, queue size: {}", _messageQueue.size());

    if (_messageQueue.size() == 1) {
        lock.unlock();
        _consume.notify_one();
        spdlog::debug("[LogicSystem] Consumer thread notified");
    }
}

LogicSystem::LogicSystem():
    _b_stop(false)
{
    spdlog::info("[LogicSystem] Initializing...");
    RegisterCallBack();
    _thread = std::thread(&LogicSystem::DealMessage, this);
    spdlog::info("[LogicSystem] Worker thread started");
}

void LogicSystem::DealMessage()
{
    spdlog::info("[LogicSystem] Message processing thread started");
    while (true) {
        std::unique_lock<std::mutex> lock(_mutex);
        while (_messageQueue.empty() && !_b_stop) {
            _consume.wait(lock);
        }

        if (_b_stop) {
            spdlog::info("[LogicSystem] Stopping message processing, remaining messages: {}", _messageQueue.size());
            while (!_messageQueue.empty()) {
                auto messageNode = _messageQueue.front();

                auto callBackIter = _funcCallBack.find(messageNode->_receiveNode->GetId());
                if (callBackIter == _funcCallBack.end()) {
                    spdlog::warn("[LogicSystem] No callback found for message ID: {}", messageNode->_receiveNode->GetId());
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
        spdlog::debug("[LogicSystem] Processing message ID: {}", messageNode->_receiveNode->GetId());

        auto callBackIter = _funcCallBack.find(messageNode->_receiveNode->GetId());
        if (callBackIter == _funcCallBack.end()) {
            spdlog::warn("[LogicSystem] No handler registered for message ID: {}", messageNode->_receiveNode->GetId());
            _messageQueue.pop();
            continue;
        }
        callBackIter->second(
            messageNode->_session,
            messageNode->_receiveNode->GetId(),
            std::string(messageNode->_receiveNode->_data, messageNode->_receiveNode->_currentLength)
        );
        _messageQueue.pop();
        spdlog::debug("[LogicSystem] Message processed, remaining queue size: {}", _messageQueue.size());
    }
}

void LogicSystem::RegisterCallBack()
{
    spdlog::info("[LogicSystem] Registering message callbacks...");
    auto id = static_cast<size_t>(MessageID::MESSAGE_CHAT_LOGIN);
    _funcCallBack[id] = std::bind(&LogicSystem::LoginHandler,this,
            std::placeholders::_1,
            std::placeholders::_2,
            std::placeholders::_3);
    spdlog::info("[LogicSystem] Registered login handler for message ID: {}", id);
}

void LogicSystem::LoginHandler(std::shared_ptr<CSession> session, const size_t& messageId, const std::string& messageData)
{
    spdlog::info("[LogicSystem] Processing login request...");
    Json::Reader reader;
    Json::Value root;
    reader.parse(messageData, root);

    auto uid = root["uid"].asString();
    auto token = root["token"].asString();
    spdlog::info("[LogicSystem] Login attempt - UID: {}, Token length: {}", uid, token.length());
    

    Json::Value rootValue;

    std::string tokenKey = ChatServerConstant::USER_TOKEN_PREFIX + uid;

    auto tokenValue = RedisConPool::GetInstance().get(tokenKey).value();
    if (tokenValue.empty()) {
		spdlog::error("[LogicSystem] Token not found in Redis for UID: {}", uid);
        rootValue["error"] = static_cast<size_t>(ErrorCodes::UID_INVALID);
        std::string returnStr = rootValue.toStyledString();
        session->Send(returnStr, static_cast<size_t>(MessageID::MESSAGE_CHAT_LOGIN_RESPONSE));
        return;
    }

    if (tokenValue != token) {
        spdlog::error("[LogicSystem] Token mismatch for UID: {}", uid);
        rootValue["error"] = static_cast<size_t>(ErrorCodes::TOKEN_INVALID);
        std::string returnStr = rootValue.toStyledString();
        session->Send(returnStr, static_cast<size_t>(MessageID::MESSAGE_CHAT_LOGIN_RESPONSE));
		return;
    }

    rootValue["error"] = static_cast<size_t>(ErrorCodes::SUCCESS);

    std::string baseKey = ChatServerConstant::USER_INFO_PREFIX + uid;
    auto userInfo = std::make_shared<UserInfo>();

    auto baseInfoExists = GetUserInfo(baseKey, uid, userInfo);

    if (!baseInfoExists) {
		spdlog::error("[LogicSystem] User info not found for UID: {}", uid);
        rootValue["error"] = static_cast<size_t>(ErrorCodes::UID_INVALID);
        std::string returnStr = rootValue.toStyledString();
        session->Send(returnStr, static_cast<size_t>(MessageID::MESSAGE_CHAT_LOGIN_RESPONSE));
        return;
    }

    rootValue["uid"] = uid;
    rootValue["username"] = userInfo->_name;
    rootValue["email"] = userInfo->_email;
    rootValue["password"] = userInfo->_password;


    auto serverName = ConfigManager::GetInstance().getValue("SelfServer","name");
    
    auto redisResult = RedisConPool::GetInstance().hget(ChatServerConstant::LOGIN_COUNT, serverName).value();
    int loginCount = 0;

    if (!redisResult.empty()) {
        loginCount = std::stoi(redisResult);
    }

    loginCount++;

	RedisConPool::GetInstance().hset(ChatServerConstant::LOGIN_COUNT, serverName, std::to_string(loginCount));

    session->SetUid(uid);

	std::string ipKey = ChatServerConstant::USER_IP_PREFIX + uid;
    RedisConPool::GetInstance().set(ipKey, serverName);

    UserManager::GetInstance()->setUserSession(uid,session);


    std::string returnStr = rootValue.toStyledString();
    session->Send(returnStr, static_cast<size_t>(MessageID::MESSAGE_CHAT_LOGIN_RESPONSE));
    spdlog::info("[LogicSystem] Login successful for UID: {}", uid);

    return;
}

bool LogicSystem::GetUserInfo(std::string baseKey, std::string uid, std::shared_ptr<UserInfo>& userInfo)
{
	auto baseInfo = RedisConPool::GetInstance().get(baseKey).value();

	if (!baseInfo.empty()) {
		Json::Reader reader;
		Json::Value root;
		reader.parse(baseInfo, root);

		userInfo->_uid = root["uid"].asString();
		userInfo->_name = root["username"].asString();
		userInfo->_email = root["email"].asString();
		userInfo->_password = root["password"].asString();
		spdlog::info("[LogicSystem] Retrieved user info for uid: {}", uid);
		return true;
	}
	else {
		spdlog::error("[LogicSystem] No user info found for uid: {}", uid);

		std::shared_ptr<UserInfo> tmp_userInfo = nullptr;
		tmp_userInfo = MySQLManager::GetInstance()->GetUser(uid);

		if (tmp_userInfo == nullptr) {
			spdlog::error("[LogicSystem] No user found in MySQL for uid: {}", uid);
			return false;
		}

		userInfo = tmp_userInfo;

		Json::Value redisRoot;
		redisRoot["uid"] = userInfo->_uid;
		redisRoot["username"] = userInfo->_name;
		redisRoot["email"] = userInfo->_email;
		redisRoot["password"] = userInfo->_password;
		std::string redisString = redisRoot.toStyledString();
		RedisConPool::GetInstance().set(baseKey, redisString);
		spdlog::info("[LogicSystem] Cached user info for uid: {}", uid);
	}

	return true;
}
