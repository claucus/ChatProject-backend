#pragma once

#include "LogicNode.h"
#include "Singleton.h"
#include "UserInfo.h"
#include <atomic>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

using FunCallBack = std::function<void(std::shared_ptr<CSession>, const size_t& messageId, const std::string& messageData)>;

class LogicSystem:public Singleton<LogicSystem>
{
	friend class Singleton<LogicSystem>;
public:
	~LogicSystem();
	void PostMessageToQueue(std::shared_ptr<LogicNode> message);

private:
	LogicSystem();
	void DealMessage();
	void RegisterCallBack();
	void LoginHandler(std::shared_ptr<CSession> session, const size_t& messageId, const std::string& messageData);
	void SearchHandler(std::shared_ptr<CSession> session, const size_t& messageId, const std::string& messageData);
	void ApplyFriendHandler(std::shared_ptr<CSession> session, const size_t& messageId, const std::string& messageData);
	void ApprovalFriendHandler(std::shared_ptr<CSession> session, const size_t& messageId, const std::string& messageData);

	bool GetUserInfo(std::string baseKey, std::string uid, std::shared_ptr<UserInfo>& userInfo);
	
private:
	std::thread _thread;
	std::mutex _mutex;
	std::condition_variable _consume;
	std::atomic<bool> _b_stop;
	std::queue < std::shared_ptr<LogicNode>> _messageQueue;

	std::map<size_t, FunCallBack> _funcCallBack;
	std::unordered_map < std::string, std::shared_ptr<UserInfo> > _users;
};