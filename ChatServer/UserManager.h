#pragma once
#include <memory>
#include <mutex>
#include <unordered_map>
#include "Singleton.h"

class CSession;
class UserManager :public Singleton<UserManager>
{
	friend class Singleton<UserManager>;
public:
	~UserManager();
	std::shared_ptr<CSession> GetSession(std::string uid);
	void setUserSession(std::string uid, std::shared_ptr<CSession> session);
	void removeUserSession(std::string sessionId);

private:
	UserManager();
	std::mutex _mutex;
	std::unordered_map<std::string, std::shared_ptr<CSession> > _uidToSession;
};

