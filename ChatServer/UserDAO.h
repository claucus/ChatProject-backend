#pragma once
#include "BaseDAO.h"
#include "UserInfo.h"
#include <mysqlx/xdevapi.h>

class UserDAO :public BaseDAO<UserInfo>
{
public:
	bool Insert(const UserInfo& user) override;
	bool Update(const UserInfo& user) override;
	bool Delete(const std::string& uid) override;
	std::unique_ptr<UserInfo> Search(const std::string& uid) override;


	bool VerifyUser(const std::string& email, const std::string& password, std::string& uid);
private:
	const std::string ENCRYPTION_KEY = "printf-secret-key";
};

