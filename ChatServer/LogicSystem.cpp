#include "LogicSystem.h"
#include "StatusGrpcClient.h"
#include "MySQLManager.h"

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
    
    auto response = StatusGrpcClient::GetInstance()->Login(uid,token);
    Json::Value rootValue;

    rootValue["error"] = response.error();
    if (response.error() != static_cast<int>(ErrorCodes::SUCCESS)) {
        spdlog::error("[LogicSystem] Login failed - Error code: {}", response.error());
        std::string returnStr = rootValue.toStyledString();
        session->Send(returnStr, static_cast<size_t>(MessageID::MESSAGE_CHAT_LOGIN_RESPONSE));
        return;
    }

    auto findIter = _users.find(uid);
    std::shared_ptr<UserInfo> userInfo = nullptr;
    if (findIter == _users.end()) {
        spdlog::debug("[LogicSystem] New user login, fetching user info from database...");
        auto uniqueUser = MySQLManager::GetInstance()->GetUser(uid);
        if (uniqueUser == nullptr) {
            spdlog::error("[LogicSystem] User info not found in database for UID: {}", uid);
            rootValue["error"] = static_cast<int> (ErrorCodes::UID_INVALID);
            std::string returnStr = rootValue.toStyledString();
            session->Send(returnStr, static_cast<size_t>(MessageID::MESSAGE_CHAT_LOGIN_RESPONSE));
            return;
        }
        
        userInfo = std::move(uniqueUser);
        _users[uid] = userInfo;
        spdlog::debug("[LogicSystem] User info cached for UID: {}", uid);
    }
    else {
        spdlog::debug("[LogicSystem] Using cached user info for UID: {}", uid);
        userInfo = findIter->second;
    }

    rootValue["uid"] = uid;
    rootValue["token"] = response.token();

    std::string returnStr = rootValue.toStyledString();
    session->Send(returnStr, static_cast<size_t>(MessageID::MESSAGE_CHAT_LOGIN_RESPONSE));
    spdlog::info("[LogicSystem] Login successful for UID: {}", uid);
}
