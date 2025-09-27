#include "UserDAO.h"
#include <iostream>
#include "Logger.h"
#include "Defer.h"

bool UserDAO::Insert(const UserInfo& user)
{
    auto conn = GetConnection();
    defer{
        ReleaseConnection(std::move(conn));
    };

    try {
        LOG_INFO("Inserting user: uid={}, email={}", user._uid, user._email);
        auto result = conn->sql("CALL sp_insert_user(?, ?, ?, ?, ?, ?, ?, ?, @success)")
            .bind(user._uid)
            .bind(user._email)
            .bind(user._username)
            .bind(user._password)
            .bind(ENCRYPTION_KEY)
            .bind(user._birth.empty() ? "1900-1-1" : user._birth)
            .bind(user._sex.empty() ? "unknown" : user._sex)
            .bind(user._avatar.empty() ? ("./avatars/" + user._uid + ".png") : user._avatar)
            .execute();

        auto statusResult = conn->sql("SELECT @success").execute();
        auto row = statusResult.fetchOne();
        bool success = row[0].get<bool>();

        
        if (success) {
            LOG_INFO("Insert user success: uid={}", user._uid);
        } else {
            LOG_WARN("Insert user failed: uid={}", user._uid);
        }
        return success;
    }
    catch (const mysqlx::Error& error) {
        LOG_ERROR("MySQL Error on insert: {} (uid={}, email={})", error.what(), user._uid, user._email);
        return false;
    }
}

bool UserDAO::Update(const UserInfo& user)
{
    auto conn = GetConnection();
	defer{
	    ReleaseConnection(std::move(conn));
	};

    try {
        LOG_INFO("Updating user: uid={}, email={}", user._uid, user._email);
        auto result = conn->sql("CALL sp_update_user(?, ?, ?, ?, ?, ?, ?, @success)")
            .bind(user._uid)
            .bind(user._email)
            .bind(user._password)
            .bind(ENCRYPTION_KEY)
            .bind(user._birth.empty() ? "1900-1-1" : user._birth)
            .bind(user._sex.empty() ? "unknown" : user._sex)
            .bind(user._avatar.empty() ? ("./avatars/" + user._uid + ".png") : user._avatar)
            .execute();

        auto statusResult = conn->sql("SELECT @success").execute();
        auto row = statusResult.fetchOne();
        bool success = row[0].get<bool>();

        if (success) {
            LOG_INFO("Update user success: uid={}", user._uid);
        } else {
            LOG_WARN("Update user failed: uid={}", user._uid);
        }
        return success;
    }
    catch (const mysqlx::Error& error) {
        LOG_ERROR("MySQL Error on update: {} (uid={}, email={})", error.what(), user._uid, user._email);
        return false;
    }
}

bool UserDAO::Delete(const std::string& uid)
{
    auto conn = GetConnection();
	defer{
	    ReleaseConnection(std::move(conn));
	};

    try {
        LOG_INFO("Deleting user: uid={}", uid);
        auto result = conn->sql("CALL sp_delete_user(?, @success)")
            .bind(uid)
            .execute();

        auto statusResult = conn->sql("SELECT @success").execute();
        auto row = statusResult.fetchOne();
        bool success = row && row[0].get<bool>();

        if (success) {
            LOG_INFO("Delete user success: uid={}", uid);
        } else {
            LOG_WARN("Delete user failed: uid={}", uid);
        }
        return success;
    }
    catch (const mysqlx::Error& error) {
        LOG_ERROR("MySQL Error on delete: {} (uid={})", error.what(), uid);
        return false;
    }
}

std::unique_ptr<UserInfo> UserDAO::Search(const std::string& uid)
{
    auto conn = GetConnection();
	defer{
	    ReleaseConnection(std::move(conn));
	};

    try {
        LOG_INFO("Searching user: uid={}", uid);
        auto result = conn->sql("CALL sp_search_user(?, ?, @success)")
            .bind(uid)
            .bind(ENCRYPTION_KEY)
            .execute();

        auto statusResult = conn->sql("SELECT @success").execute();    
        auto statusRow = statusResult.fetchOne();
        bool found = statusRow && statusRow[0].get<bool>();

        if (!found) {
            LOG_WARN("User not found: uid={}", uid);
            return nullptr; // 用户未找到
        }

        auto row = result.fetchOne();
        if (row) {
            auto userInfo = std::make_unique<UserInfo>(
                row[0].get<std::string>(),  // uid
                row[1].get<std::string>(),  // email
                row[2].get<std::string>(),  // name
                row[3].get<std::string>(),  // password (已解密)
                row[4].isNull() ? "" : row[4].get<std::string>(),// birth
                row[6].isNull() ? "" : row[6].get<std::string>(), //avatar
                row[5].isNull() ? "" : row[5].get<std::string>() //sex
            );
            LOG_INFO("User found: uid={}, email={}", userInfo->_uid, userInfo->_email);
            return userInfo;
        }

        LOG_WARN("User row not found after success: uid={}", uid);
        return nullptr;
    }
    catch (const mysqlx::Error& error) {
        LOG_ERROR("MySQL Error on search: {} (uid={})", error.what(), uid);
        return nullptr;
    }
}

bool UserDAO::VerifyUser(const std::string& email, const std::string& password, std::string& uid)
{
    auto conn = GetConnection();
	defer{
	    ReleaseConnection(std::move(conn));
	};

    try {
        LOG_INFO("Verifying user: email={}", email);
        auto result = conn->sql("CALL sp_verify_user(?,?,?,@out_uid,@success)")
            .bind(email)
            .bind(password)
            .bind(ENCRYPTION_KEY)
            .execute();

        auto statusResult = conn->sql("SELECT @success, @out_uid").execute();
        auto row = statusResult.fetchOne();

        if (row && row[0].get<bool>()) {
            uid = row[1].get<std::string>();
            LOG_INFO("Verify user success: email={}, uid={}", email, uid);
            return true;
        }

        LOG_WARN("Verify user failed: email={}", email);
        return false;
    }
    catch (const mysqlx::Error& error) {
        LOG_ERROR("MySQL Error on verify: {} (email={})", error.what(), email);
        return false;
    }
}
