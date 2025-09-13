#include "FriendDAO.h"
#include <iostream>
#include "Logger.h"
#include "Defer.h"


bool FriendDAO::Insert(const FriendRelation& relation)
{
	auto conn = GetConnection();
	defer{
		ReleaseConnection(std::move(conn));
	};

	try {
		LOG_INFO("Inserting relationship: uid_a={}, uid_b={}", relation._a_uid, relation._b_uid);
		auto result = conn->sql("CALL sp_insert_friend(?, ?, ?, ?, ?, @success)")
			.bind(relation._a_uid)
			.bind(relation._b_uid)
			.bind(relation._status)
			.bind(relation._group_a.empty() ? "wodehaoyou" : relation._group_a)
			.bind(relation._group_b.empty() ? "wodehaoyou" : relation._group_b)
			.execute();

		auto statusResult = conn->sql("SELECT @success").execute();
		auto row = statusResult.fetchOne();
		bool success = row[0].get<bool>();

		if (success) {
			LOG_INFO("Insert relationship success: uid_a={}, uid_b={}", relation._a_uid, relation._b_uid);
		}
		else {
			LOG_WARN("Insert relationship failed: uid_a={}, uid_b={}", relation._a_uid, relation._b_uid);
		}
		return success;
	}
	catch (const mysqlx::Error& error) {
		LOG_ERROR("MySQL Error on insert: {} (uid_a={}, uid_b={})", error.what(), relation._a_uid, relation._b_uid);
		return false;
	}
}

bool FriendDAO::Update(const FriendRelation& relation)
{
	auto conn = GetConnection();
	defer{
		ReleaseConnection(std::move(conn));
	};

	try {
		LOG_INFO("Updating relationship: uid_a={}, uid_b={}", relation._a_uid, relation._b_uid);
		auto result = conn->sql("CALL sp_update_friend(?, ?, ?, ?, ?, @success)")
			.bind(relation._a_uid)
			.bind(relation._b_uid)
			.bind(relation._status)
			.bind(relation._group_a.empty() ? "wodehaoyou" : relation._group_a)
			.bind(relation._group_b.empty() ? "wodehaoyou" : relation._group_b)
			.execute();

		auto statusResult = conn->sql("SELECT @success").execute();
		auto row = statusResult.fetchOne();
		bool success = row[0].get<bool>();

		if (success) {
			LOG_INFO("Update relationship success: uid_a={}, uid_b={}", relation._a_uid, relation._b_uid);
		}
		else {
			LOG_WARN("Update relationship failed: uid_a={}, uid_b={}", relation._a_uid, relation._b_uid);
		}
		return success;

	}
	catch (const mysqlx::Error& error) {
		LOG_ERROR("MySQL Error on update: {} (uid_a={}, uid_b={})", error.what(), relation._a_uid, relation._b_uid);
		return false;
	}
}

bool FriendDAO::Delete(const std::string& uid)
{
	return true;
}

std::unique_ptr<FriendRelation> FriendDAO::Search(const std::string& uid)
{
	return nullptr;
}

bool FriendDAO::DeleteFriendShip(const std::string& a_uid, const std::string& b_uid)
{
	auto conn = GetConnection();
	defer{
		ReleaseConnection(std::move(conn));
	};

	try {
		LOG_INFO("Delete relationship: uid_a={}, uid_b={}",a_uid, b_uid);
		auto result = conn->sql("CALL sp_delete_friend(?, ?, @success)")
			.bind(a_uid)
			.bind(b_uid)
			.execute();

		auto statusResult = conn->sql("SELECT @success").execute();
		auto row = statusResult.fetchOne();
		bool success = row && row[0].get<bool>();

		if (success) {
			LOG_INFO("Delete relationship success: uid_a={}, uid_b={}", a_uid, b_uid);
		}
		else {
			LOG_WARN("Delete relationship failed: uid_a={}, uid_b={}", a_uid, b_uid);
		}
		return success;
	}
	catch (const mysqlx::Error& error) {
		LOG_ERROR("MySQL Error on Delete: {} ( uid_a={}, uid_b={} )",error.what(), a_uid, b_uid);
		return false;
	}
}

std::vector<std::shared_ptr<FriendRelation>> FriendDAO::GetUserFriends(const std::string& uid)
{
	auto conn = GetConnection();
	std::vector<std::shared_ptr<FriendRelation>> friends;
	defer{
		ReleaseConnection(std::move(conn));
	};

	try {
		LOG_INFO("Finding relationship: uid={}",uid);
		auto result = conn->sql("CALL sp_search_friend(?, @success)")
			.bind(uid)
			.execute();

		auto statusResult = conn->sql("SELECT @success").execute();
		auto statusRow = statusResult.fetchOne();
		bool found = statusRow && statusRow[0].get<bool>();

		if (!found) {
			LOG_WARN("Get User Friends failed: uid={}",uid);
		}
		else {
			for (auto row : result) {
				auto relationship = std::make_shared<FriendRelation>(
					row[0].get<std::string>(),
					row[1].get<std::string>(),
					row[2].get<int>(),
					row[3].isNull() ? "" : row[3].get<std::string>(),
					row[4].isNull() ? "" : row[4].get<std::string>()
				);

				friends.emplace_back(std::move(relationship));
			}
			LOG_INFO("Get User Friends success: uid={}, entries={}", uid,friends.size());
		}
	}
	catch (const mysqlx::Error& error) {
		LOG_ERROR("MySQL Error on Delete: {} ( uid={} )", error.what(), uid);
	}
	return friends;
}

std::vector<std::shared_ptr<SearchInfo>> FriendDAO::Search(const std::string& uid, const std::string& pattern)
{
	auto conn = GetConnection();
	std::vector<std::shared_ptr<SearchInfo>> results;

	defer{
		ReleaseConnection(std::move(conn));
	};

	try {
		LOG_INFO("Finding User by Fuzzy Search: uid={}, pattern={}", uid, pattern);
		auto result = conn->sql("CALL sp_fuzzy_search_uid_email(?,?,@success)")
			.bind(uid)
			.bind(pattern)
			.execute();

		auto statusResult = conn->sql("SELECT @success").execute();
		auto statusRow = statusResult.fetchOne();
		bool found = statusRow && statusRow[0].get<bool>();

		if (!found) {
			LOG_WARN("Fuzzy Search failed: uid={}, pattern={}", uid, pattern);
		}
		else {
			for (auto row : result) {
				auto user = std::make_shared<SearchInfo>(
					row[0].get<std::string>(),
					row[1].get<std::string>(),
					row[2].get<std::string>(),
					row[3].get<int>()
				);

				results.emplace_back(std::move(user));
			}
			LOG_INFO("Fuzzy Search success: uid={}, pattern={}, entries={}", uid, pattern, results.size());
		}
	}
	catch (const mysqlx::Error& error) {
		LOG_ERROR("MySQL Error on Fuzzy Search: {} (uid={}, pattern={})", error.what(), uid, pattern);
	}
	return results;
}