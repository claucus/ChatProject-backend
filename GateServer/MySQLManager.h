#pragma once
#include "Singleton.h"
#include "UserDAO.h"
#include "FriendDAO.h"

class MySQLManager :public Singleton<MySQLManager>
{
	friend class Singleton<MySQLManager>;

public:
	~MySQLManager() = default;
	bool RegisterUser(UserInfo& user);
	bool ResetPassword(UserInfo& user);
	bool UserLogin(UserInfo& user);
	std::unique_ptr<UserInfo> GetUser(const std::string& uid);
	std::vector <std::shared_ptr<SearchInfo>> FuzzySearchUsers(const std::string& uid,const std::string& pattern);

	bool AddFriend(FriendRelation& relation);
private:
	MySQLManager() = default;

	UserDAO _userDAO;
	FriendDAO _friendDAO;
};

