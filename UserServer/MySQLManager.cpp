#include "MySQLManager.h"
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <random>
#include "Logger.h"

bool MySQLManager::RegisterUser(UserInfo& user)
{
	LOG_INFO("Processing user registration request - Email: {}", user.email);
	auto verify = _userDAO.VerifyUser(user.email, user.password, user.uid);
	if (verify) {
		LOG_WARN("Registration failed - User already exists - Email: {}", user.email);
		return false;
	}

	if (user.uid.empty()) {

		auto rnd = [](int x)->int {
			std::mt19937 mrand(std::random_device{}());
			return mrand() % x;
			};

		auto a_uuid = boost::uuids::random_generator()();
		auto uid = boost::uuids::to_string(a_uuid);
		uid.erase(
			remove_if(uid.begin(), uid.end(), [](char ch) {return (ch == '-'); }),
			uid.end()
		);
		for (auto& ch : uid) {
			if (!isdigit(ch)) {
				ch = ch - 'a';
				ch = ch + '0';
			}
		}


		user.uid = uid.substr(rnd(18), static_cast<std::basic_string<char, std::char_traits<char>, std::allocator<char>>::size_type>(7) + rnd(4));

		if (user.uid.front() == '0') {
			user.uid.front() += rnd(9) + 1;
		}

		LOG_DEBUG("Generated new UID for user: {}", user.uid);
	}

	try {
		_userDAO.Insert(user);
		LOG_INFO("User registration successful - Email: {}, UID: {}", user.email, user.uid);
		return true;
	}
	catch (const std::exception& e) {
		LOG_ERROR("User registration failed - Email: {}, Error: {}", user.email, e.what());
		return false;
	}
}

bool MySQLManager::ResetPassword(UserInfo& user)
{
	LOG_INFO("Processing password reset request - UID: {}", user.uid);

	try {
		bool result = _userDAO.Update(user);
		if (result) {
			LOG_INFO("Password reset successful - UID: {}", user.uid);
		}
		else {
			LOG_WARN("Password reset failed - User not found - UID: {}", user.uid);
		}
		return result;
	}
	catch (const std::exception& e) {
		LOG_ERROR("Password reset failed - UID: {}, Error: {}", user.uid, e.what());
		return false;
	}
}

bool MySQLManager::UserLogin(UserInfo& user)
{
	if (!user.email.empty()) {
		LOG_DEBUG("Attempting login with email: {}", user.email);
		auto verify = _userDAO.VerifyUser(user.email, user.password, user.uid);
		if (verify) {
			LOG_INFO("Login successful - Email: {}", user.email);
		}
		else {
			LOG_WARN("Login failed - Invalid credentials - Email: {}", user.email);
		}
		return verify;
	}

	if (!user.uid.empty()) {
		LOG_DEBUG("Attempting login with UID: {}", user.uid);
		auto sqlUser = _userDAO.Search(user.uid);
		if (sqlUser == nullptr) {
			LOG_WARN("Login failed - User not found - UID: {}", user.uid);
			return false;
		}

		if (sqlUser->password == user.password) {
			user.email = sqlUser->email;
			LOG_INFO("Login successful - UID: {}, Email: {}", user.uid, user.email);
			return true;
		}

		LOG_WARN("Login failed - Invalid password - UID: {}", user.uid);
	}
	return false;
}

std::unique_ptr<UserInfo> MySQLManager::GetUser(const std::string& uid)
{
	LOG_DEBUG("Fetching user info - UID: {}", uid);

	auto user = _userDAO.Search(uid);
	if (user) {
		LOG_DEBUG("User info found - UID: {}, Email: {}", uid, user->email);
	}
	else {
		LOG_WARN("User info not found - UID: {}", uid);
	}
	return user;
}


