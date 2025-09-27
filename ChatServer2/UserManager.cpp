#include "UserManager.h"
#include "CSession.h"

UserManager::~UserManager()
{
	_uidToSession.clear();
}

std::shared_ptr<CSession> UserManager::GetSession(std::string uid)
{
	std::lock_guard<std::mutex> lock(_mutex);
	auto iter = _uidToSession.find(uid);
	if (iter == _uidToSession.end()) {
		return nullptr;
	}
	
	return iter->second;
}

void UserManager::setUserSession(std::string uid, std::shared_ptr<CSession> session)
{
	std::lock_guard<std::mutex> lock(_mutex);
	_uidToSession[uid] = session;
}

void UserManager::removeUserSession(std::string sessionId)
{
	{
		std::lock_guard<std::mutex> lock(_mutex);
		_uidToSession.erase(sessionId);
	}
}

UserManager::UserManager()
{

}