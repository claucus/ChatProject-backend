#pragma once
#include "Singleton.h"
#include "UserDAO.h"

class MySQLManager :public Singleton<MySQLManager>
{
	friend class Singleton<MySQLManager>;

public:
	~MySQLManager() = default;
	bool RegisterUser(UserInfo& user);
	bool ResetPassword(UserInfo& user);
	bool UserLogin(UserInfo& user);
	std::unique_ptr<UserInfo> GetUser(const std::string& uid);

private:
	MySQLManager() = default;

	UserDAO _userDAO;
};

