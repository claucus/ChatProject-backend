#pragma once
#include <string>
#include <vector>
#include <memory>
#include "MySQLConPool.h"

template <typename T>
class BaseDAO
{
public:
	virtual ~BaseDAO() = default;
	
	virtual bool Insert(const T&) = 0;
	virtual bool Update(const T&) = 0;
	virtual bool Delete(const std::string&) = 0;
	virtual std::unique_ptr<T> Search(const std::string&) = 0;

protected:
	std::unique_ptr<mysqlx::Session> GetConnection() {
		return MySQLConPool::GetInstance().GetConnection();
	}
	void ReleaseConnection(std::unique_ptr<mysqlx::Session> conn) {
		MySQLConPool::GetInstance().ReleaseConnection(std::move(conn));
	}
};

