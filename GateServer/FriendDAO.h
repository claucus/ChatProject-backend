#pragma once
#include "BaseDAO.h"
#include "UserInfo.h"

#include <mysqlx/xdevapi.h>
class FriendDAO :public BaseDAO<FriendRelation>
{
public:
	bool Insert(const FriendRelation& relation) override;
	bool Update(const FriendRelation& relation) override;
	bool Delete(const std::string& uid) override;
	std::unique_ptr<FriendRelation> Search(const std::string& uid) override;


	bool DeleteFriendShip(const std::string& a_uid, const std::string& b_uid);
	std::vector<std::shared_ptr<FriendInfo>> GetUserFriends(const std::string& uid);
	std::vector<std::shared_ptr<FriendListInfo>> GetApplyList(const std::string& uid);
	std::vector<std::shared_ptr<SearchInfo>> Search(const std::string& uid, const std::string& pattern);

private:
	const std::string ENCRYPTION_KEY = "print-secret-key";
};

