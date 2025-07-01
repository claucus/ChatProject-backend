#include "MySQLManager.h"
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <random>
#include <spdlog/spdlog.h>

bool MySQLManager::RegisterUser(UserInfo& user)
{
    spdlog::info("[MySQLManager] Processing user registration request - Email: {}", user._email);
	auto verify = _userDAO.VerifyUser(user._email, user._password, user._uid);
	if (verify) {
        spdlog::warn("[MySQLManager] Registration failed - User already exists - Email: {}", user._email);
		return false;
	}

	if (user._uid.empty()) {
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
		auto rnd = [](int x)->int {
			std::mt19937 mrand(std::random_device{}());
			return mrand() % x;
			};
		user._uid = uid.substr(0, static_cast<std::basic_string<char, std::char_traits<char>, std::allocator<char>>::size_type>(7) + rnd(3));
        spdlog::debug("[MySQLManager] Generated new UID for user: {}", user._uid);
	}

    try {
        _userDAO.Insert(user);
        spdlog::info("[MySQLManager] User registration successful - Email: {}, UID: {}",
            user._email, user._uid);
        return true;
    }
    catch (const std::exception& e) {
        spdlog::error("[MySQLManager] User registration failed - Email: {}, Error: {}",
            user._email, e.what());
        return false;
    }
}

bool MySQLManager::ResetPassword(UserInfo& user)
{
    spdlog::info("[MySQLManager] Processing password reset request - UID: {}", user._uid);

    try {
        bool result = _userDAO.Update(user);
        if (result) {
            spdlog::info("[MySQLManager] Password reset successful - UID: {}", user._uid);
        }
        else {
            spdlog::warn("[MySQLManager] Password reset failed - User not found - UID: {}", user._uid);
        }
        return result;
    }
    catch (const std::exception& e) {
        spdlog::error("[MySQLManager] Password reset failed - UID: {}, Error: {}",
            user._uid, e.what());
        return false;
    }
}

bool MySQLManager::UserLogin(UserInfo& user)
{
    if (!user._email.empty()) {
        spdlog::debug("[MySQLManager] Attempting login with email: {}", user._email);
        auto verify = _userDAO.VerifyUser(user._email, user._password, user._uid);
        if (verify) {
            spdlog::info("[MySQLManager] Login successful - Email: {}", user._email);
        }
        else {
            spdlog::warn("[MySQLManager] Login failed - Invalid credentials - Email: {}", user._email);
        }
        return verify;
    }

    if (!user._uid.empty()) {
        spdlog::debug("[MySQLManager] Attempting login with UID: {}", user._uid);
        auto sqlUser = _userDAO.Search(user._uid);
        if (sqlUser == nullptr) {
            spdlog::warn("[MySQLManager] Login failed - User not found - UID: {}", user._uid);
            return false;
        }

        if (sqlUser->_password == user._password) {
            user._email = sqlUser->_email;
            spdlog::info("[MySQLManager] Login successful - UID: {}, Email: {}",
                user._uid, user._email);
            return true;
        }

        spdlog::warn("[MySQLManager] Login failed - Invalid password - UID: {}", user._uid);
    }
    return false;
}

std::unique_ptr<UserInfo> MySQLManager::GetUser(const std::string& uid)
{
    spdlog::debug("[MySQLManager] Fetching user info - UID: {}", uid);

    auto user = _userDAO.Search(uid);
    if (user) {
        spdlog::debug("[MySQLManager] User info found - UID: {}, Email: {}",
            uid, user->_email);
    }
    else {
        spdlog::warn("[MySQLManager] User info not found - UID: {}", uid);
    }
    return user;
}
