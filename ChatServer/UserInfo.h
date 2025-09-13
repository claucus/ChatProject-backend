#pragma once
#include <string>

class UserInfo {
public:
	UserInfo() = default;
	UserInfo(const std::string& uid, const std::string& email,
		const std::string& username, const std::string& password,
		const std::string& birth = "", const std::string avatar = "", const std::string& sex = "")
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
	std::string _avatar;
	std::string _sex;
};

class SearchInfo {
public:
	SearchInfo() = default;
	SearchInfo(const std::string& uid, const std::string& username, const std::string& avatar, const int& stauts)
		: _uid(uid)
		, _username(username)
		, _avatar(avatar)
		, _status(stauts)
	{

	}

	std::string _uid;
	std::string _username;
	std::string _avatar;
	int _status;
};

class FriendRelation {
public:
	FriendRelation() = default;
	FriendRelation(const std::string& uid1, const std::string& uid2,
		const int& status,
		const std::string& group1 = "", const std::string& remark1 = "",
		const std::string& group2 = "", const std::string remark2 = "")
		: _a_uid(uid1)
		, _b_uid(uid2)
		, _status(status)
		, _group_a(group1)
		, _remark_a(remark1)
		, _group_b(group2)
		, _remark_b(remark2)
	{

	}


	std::string _a_uid;
	std::string _b_uid;
	int _status;
	std::string _group_a;
	std::string _remark_a;
	std::string _group_b;
	std::string _remark_b;
};