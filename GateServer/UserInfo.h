#pragma once
#include <string>

class UserInfo {
public:
	UserInfo() = default;
	UserInfo(const std::string& uid, const std::string& email, const std::string& username, const std::string& password, const std::string& birth = "", const std::vector<char>& avatar = std::vector<char>(), const std::string& sex = "")
		: _uid(uid)
		, _email(email)
		, _username(username)
		, _password(password)
		, _birth(birth)
		, _avatar(avatar)
		, _sex(sex)
	{

	}

	std::string _uid;
	std::string _email;
	std::string _username;
	std::string _password;
	std::string _birth;
	std::vector<char> _avatar;
	std::string _sex;
};