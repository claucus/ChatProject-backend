#pragma once
#include <string>
#include "const.h"

struct UserInfo {
	std::string uid;
	std::string email;
	std::string username;
	std::string password;
	std::string birth;
	std::string avatar;
	std::string sex;

	UserInfo() = default;
	UserInfo(const std::string& uid, const std::string& email,
		const std::string& username, const std::string& password,
		const std::string& birth = "", const std::string& avatar = "", const std::string& sex = "")
		: uid(uid)
		, email(email)
		, username(username)
		, password(password)
		, birth(birth)
		, avatar(avatar)
		, sex(sex)
	{
	}
};