#pragma once
#include <string>
#include "const.h"

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
		const std::string& group = "", const std::string& remark = "")
		: _a_uid(uid1)
		, _b_uid(uid2)
		, _status(status)
		, _group(group)
		, _remark(remark)
	{

	}


	std::string _a_uid;
	std::string _b_uid;
	int _status;
	std::string _group;
	std::string _remark;
};


class FriendListInfo {
public:
	FriendListInfo() = default;
	FriendListInfo(const std::string& uid, const std::string& username,
		const std::string& avatar, const std::string& comments,
		const size_t& time, const int& status)
		: _uid(uid)
		, _username(username)
		, _avatar(avatar)
		, _comments(comments)
		, _time(time)
		, _status(status)
	{
	}

	std::string _username;
	std::string _uid;
	std::string _avatar;
	std::string _comments;
	size_t _time;
	int _status;
};

class FriendInfo {
public:
	FriendInfo() = default;
	~FriendInfo() = default;
	FriendInfo(const std::shared_ptr<UserInfo>& user,
		const std::string& group = "", const std::string& remark = "")
		: _user(user)
		, _group(group)
		, _remark(remark)
	{
	}


	std::shared_ptr<UserInfo> _user;
	std::string _group;
	std::string _remark;
};