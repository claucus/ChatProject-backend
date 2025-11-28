#pragma once  
#include <atomic>
#include <condition_variable>  
#include <memory>  
#include <mutex>  
#include <mysqlx/xdevapi.h>
#include <queue>  
#include <string>  

class MySQLConPool
{
public:
	static MySQLConPool& GetInstance();
	std::unique_ptr<mysqlx::Session> GetConnection();
	void ReleaseConnection(std::unique_ptr<mysqlx::Session>);
	~MySQLConPool();

private:
	MySQLConPool();
	MySQLConPool(const MySQLConPool&) = delete;
	MySQLConPool& operator=(const MySQLConPool&) = delete;

	std::unique_ptr<mysqlx::Session> CreateConnection();

	std::string _host;
	std::size_t _port;
	std::string _user;
	std::string _password;
	std::string _schema;
	std::size_t _poolSize;

	std::queue < std::unique_ptr<mysqlx::Session> > _pool;
	std::mutex _mutex;
	std::condition_variable _condition;
	std::atomic<bool> _b_stop;
};
