#include "UserDAO.h"
#include <iostream>
#include <spdlog/spdlog.h>
#include "Defer.h"

bool UserDAO::Insert(const UserInfo& user)
{
    auto conn = GetConnection();
    defer{
        ReleaseConnection(std::move(conn));
    };

    try {
        spdlog::info("[UserDAO] Inserting user: uid={}, email={}", user._uid, user._email);
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
            spdlog::info("[UserDAO] Insert user success: uid={}", user._uid);
        } else {
            spdlog::warn("[UserDAO] Insert user failed: uid={}", user._uid);
        }
        return success;
    }
    catch (const mysqlx::Error& error) {
        spdlog::error("[UserDAO] MySQL Error on insert: {} (uid={}, email={})", error.what(), user._uid, user._email);
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
        spdlog::info("[UserDAO] Updating user: uid={}, email={}", user._uid, user._email);
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
            spdlog::info("[UserDAO] Update user success: uid={}", user._uid);
        } else {
            spdlog::warn("[UserDAO] Update user failed: uid={}", user._uid);
        }
        return success;
    }
    catch (const mysqlx::Error& error) {
        spdlog::error("[UserDAO] MySQL Error on update: {} (uid={}, email={})", error.what(), user._uid, user._email);
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
        spdlog::info("[UserDAO] Deleting user: uid={}", uid);
        auto result = conn->sql("CALL sp_delete_user(?, @success)")
            .bind(uid)
            .execute();

        auto statusResult = conn->sql("SELECT @success").execute();
        auto row = statusResult.fetchOne();
        bool success = row && row[0].get<bool>();

        if (success) {
            spdlog::info("[UserDAO] Delete user success: uid={}", uid);
        } else {
            spdlog::warn("[UserDAO] Delete user failed: uid={}", uid);
        }
        return success;
    }
    catch (const mysqlx::Error& error) {
        spdlog::error("[UserDAO] MySQL Error on delete: {} (uid={})", error.what(), uid);
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
        spdlog::info("[UserDAO] Searching user: uid={}", uid);
        auto result = conn->sql("CALL sp_search_user(?, ?, @success)")
            .bind(uid)
            .bind(ENCRYPTION_KEY)
            .execute();

        auto statusResult = conn->sql("SELECT @success").execute();    
        auto statusRow = statusResult.fetchOne();
        bool found = statusRow && statusRow[0].get<bool>();

        if (!found) {
            spdlog::warn("[UserDAO] User not found: uid={}", uid);
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
            spdlog::info("[UserDAO] User found: uid={}, email={}", userInfo->_uid, userInfo->_email);
            return userInfo;
        }

        spdlog::warn("[UserDAO] User row not found after success: uid={}", uid);
        return nullptr;
    }
    catch (const mysqlx::Error& error) {
        spdlog::error("[UserDAO] MySQL Error on search: {} (uid={})", error.what(), uid);
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
        spdlog::info("[UserDAO] Verifying user: email={}", email);
        auto result = conn->sql("CALL sp_verify_user(?,?,?,@out_uid,@success)")
            .bind(email)
            .bind(password)
            .bind(ENCRYPTION_KEY)
            .execute();

        auto statusResult = conn->sql("SELECT @success, @out_uid").execute();
        auto row = statusResult.fetchOne();

        if (row && row[0].get<bool>()) {
            uid = row[1].get<std::string>();
            spdlog::info("[UserDAO] Verify user success: email={}, uid={}", email, uid);
            return true;
        }

        spdlog::warn("[UserDAO] Verify user failed: email={}", email);
        return false;
    }
    catch (const mysqlx::Error& error) {
        spdlog::error("[UserDAO] MySQL Error on verify: {} (email={})", error.what(), email);
        return false;
    }
}
