#include "MySQLManager.h"
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <random>
#include "Logger.h"

bool MySQLManager::RegisterUser(UserInfo& user)
{
    LOG_INFO("Processing user registration request - Email: {}", user._email);
	auto verify = _userDAO.VerifyUser(user._email, user._password, user._uid);
	if (verify) {
        LOG_WARN("Registration failed - User already exists - Email: {}", user._email);
		return false;
	}

    if (user._uid.empty()) {

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


		user._uid = uid.substr(rnd(18), static_cast<std::basic_string<char, std::char_traits<char>, std::allocator<char>>::size_type>(7) + rnd(4));

		if (user._uid.front() == '0') {
			user._uid.front() += rnd(9) + 1;
		}

        LOG_DEBUG("Generated new UID for user: {}", user._uid);
	}

    try {
        _userDAO.Insert(user);
        LOG_INFO("User registration successful - Email: {}, UID: {}", user._email, user._uid);
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR("User registration failed - Email: {}, Error: {}", user._email, e.what());
        return false;
    }
}

bool MySQLManager::ResetPassword(UserInfo& user)
{
    LOG_INFO("Processing password reset request - UID: {}", user._uid);

    try {
        bool result = _userDAO.Update(user);
        if (result) {
            LOG_INFO("Password reset successful - UID: {}", user._uid);
        }
        else {
            LOG_WARN("Password reset failed - User not found - UID: {}", user._uid);
        }
        return result;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Password reset failed - UID: {}, Error: {}", user._uid, e.what());
        return false;
    }
}

bool MySQLManager::UserLogin(UserInfo& user)
{
    if (!user._email.empty()) {
        LOG_DEBUG("Attempting login with email: {}", user._email);
        auto verify = _userDAO.VerifyUser(user._email, user._password, user._uid);
        if (verify) {
            LOG_INFO("Login successful - Email: {}", user._email);
        }
        else {
            LOG_WARN("Login failed - Invalid credentials - Email: {}", user._email);
        }
        return verify;
    }

    if (!user._uid.empty()) {
        LOG_DEBUG("Attempting login with UID: {}", user._uid);
        auto sqlUser = _userDAO.Search(user._uid);
        if (sqlUser == nullptr) {
            LOG_WARN("Login failed - User not found - UID: {}", user._uid);
            return false;
        }

        if (sqlUser->_password == user._password) {
            user._email = sqlUser->_email;
            LOG_INFO("Login successful - UID: {}, Email: {}", user._uid, user._email);
            return true;
        }

        LOG_WARN("Login failed - Invalid password - UID: {}", user._uid);
    }
    return false;
}

std::unique_ptr<UserInfo> MySQLManager::GetUser(const std::string& uid)
{
    LOG_DEBUG("Fetching user info - UID: {}", uid);

    auto user = _userDAO.Search(uid);
    if (user) {
        LOG_DEBUG("User info found - UID: {}, Email: {}", uid, user->_email);
    }
    else {
        LOG_WARN("User info not found - UID: {}", uid);
    }
    return user;
}

std::vector <std::shared_ptr<SearchInfo>> MySQLManager::FuzzySearchUsers(const std::string& uid, const std::string& pattern)
{
    return _friendDAO.Search(uid, pattern);
}

bool MySQLManager::AddFriend(FriendRelation& relation, const std::string& comments)
{
    try {
        auto result = _friendDAO.InsertApply(relation,comments);
        return result;
    }
    catch (const std::exception& e) {
		LOG_ERROR("AddFriend Exception: {}", e.what());
        return false;
    }
}

bool MySQLManager::UpdateFriendStatus(FriendRelation& relation)
{
    try {
		auto result = _friendDAO.Update(relation);
        return result;
    }
    catch (const std::exception& e) {
		LOG_ERROR("UpdateFriendStatus Exception: {}", e.what());
		return false;
    }
}

std::vector<std::shared_ptr<FriendListInfo>> MySQLManager::GetApplyList(const std::string& uid)
{
	return _friendDAO.GetApplyList(uid);
}
