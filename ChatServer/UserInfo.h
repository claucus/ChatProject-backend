#pragma once
#include <string>

class UserInfo {
public:
	UserInfo() = default;
	UserInfo(const std::string& name, const std::string& password,
		const std::string& email, const std::string uid) :
		_name(name), _password(password), _email(email), _uid(uid) 
	{

	}

	std::string _name;
	std::string _password;
	std::string _email;
	std::string _uid;
};