#include "LogicSystem.h"
#include "StatusGrpcClient.h"
#include "MySQLManager.h"
#include "RedisConPool.h"
#include "ConfigManager.h"
#include "UserManager.h"
#include "Defer.h"

#include <spdlog/spdlog.h>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

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

    id = static_cast<size_t>(MessageID::MESSAGE_GET_SEARCH_USER);
    _funcCallBack[id] = std::bind(&LogicSystem::SearchHandler, this,
            std::placeholders::_1,
            std::placeholders::_2,
            std::placeholders::_3);

    spdlog::info("[LogicSystem] Registered login handler for message ID: {}", id);
}

void LogicSystem::LoginHandler(std::shared_ptr<CSession> session, const size_t& messageId, const std::string& messageData)
{
    spdlog::info("[LogicSystem] Processing login request...");

    json root;
    defer{
		std::string returnStr = root.dump(4);
		session->Send(returnStr, static_cast<size_t>(MessageID::MESSAGE_CHAT_LOGIN_RESPONSE));
    };

    try {
        auto src = json::parse(messageData);

        std::string uid = src["uid"].get<std::string>();
        std::string token = src["token"].get<std::string>();
        spdlog::info("[LogicSystem] Login attempt - UID: {}, Token length: {}", uid, token.length());


        std::string tokenKey = ChatServiceConstant::USER_TOKEN_PREFIX + uid;
        auto tokenValue = RedisConPool::GetInstance().get(tokenKey).value();

		if (tokenValue.empty()) {
			spdlog::error("[LogicSystem] Token not found in Redis for UID: {}", uid);
			root["error"] = static_cast<size_t>(ErrorCodes::UID_INVALID);
			return;
		}

		if (tokenValue != token) {
			spdlog::error("[LogicSystem] Token mismatch for UID: {}", uid);
			root["error"] = static_cast<size_t>(ErrorCodes::TOKEN_INVALID);
			return;
		}


		root["error"] = static_cast<size_t>(ErrorCodes::SUCCESS);
		std::string baseKey = ChatServiceConstant::USER_INFO_PREFIX + uid;
        auto userInfo = std::make_shared<UserInfo>();
        
        auto baseInfoExists = GetUserInfo(baseKey, uid, userInfo);

		if (!baseInfoExists) {
			spdlog::error("[LogicSystem] User info not found for UID: {}", uid);
			root["error"] = static_cast<size_t>(ErrorCodes::UID_INVALID);
			return;
		}

        root["uid"] = uid;
        root["username"] = userInfo->_username;
        root["email"] = userInfo->_email;
        root["password"] = userInfo->_password;


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

		spdlog::info("[LogicSystem] Login successful for UID: {}", uid);

		return;
    }
    catch (const json::parse_error& e) {
        spdlog::warn("[LogicSystem] Failed to parse JSON in LoginHandler: {}", e.what());
        root["error"] = static_cast<int>(ErrorCodes::ERROR_JSON);
    }
}

void LogicSystem::SearchHandler(std::shared_ptr<CSession> session, const size_t& messageId, const std::string& messageData)
{

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
			spdlog::info("[LogicSystem] Retrieved user info for uid: {}", uid);
        }
        catch(const json::parse_error& e){
			spdlog::warn("[LogicSystem] Failed to parse JSON in GetUserInfo: {}", e.what());
        }
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

        json root;
        root["uid"] = userInfo->_uid;
        root["username"] = userInfo->_username;
        root["email"] = userInfo->_email;
        root["password"] = userInfo->_password;

        std::string redisString = root.dump(4);
		RedisConPool::GetInstance().set(baseKey, redisString);
		spdlog::info("[LogicSystem] Cached user info for uid: {}", uid);
	}

	return true;
}
