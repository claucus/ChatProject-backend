#include "MySQLConPool.h"
#include "ConfigManager.h"
#include "Logger.h"

MySQLConPool& MySQLConPool::GetInstance()
{
	static MySQLConPool instance;
	return instance;
}

std::unique_ptr<mysqlx::Session> MySQLConPool::GetConnection()
{
	std::unique_lock<std::mutex> lock(_mutex);
    LOG_DEBUG("[MySQLPool] Waiting for available connection, current pool size: {}", _pool.size());

	_condition.wait(
		lock, 
		[this]() {
			if (_b_stop) {
				return true;
			}
			return !_pool.empty();
		}
	);
	if (_b_stop) {
        LOG_WARN("[MySQLPool] Connection pool is stopping, returning nullptr");
		return nullptr;
	}
	std::unique_ptr<mysqlx::Session> con(std::move(_pool.front()));
	_pool.pop();
    LOG_DEBUG("[MySQLPool] Connection acquired, remaining connections: {}", _pool.size());
	return con;
}

void MySQLConPool::ReleaseConnection(std::unique_ptr<mysqlx::Session> con)
{
	if (!con) {
        LOG_WARN("[MySQLPool] Attempting to release null connection");
		return;
	}

	std::unique_lock<std::mutex> lock(_mutex);
	if (_b_stop) {
        LOG_INFO("[MySQLPool] Pool is stopping, released connection will be destroyed");
		return;
	}
	_pool.push(std::move(con));
    LOG_DEBUG("[MySQLPool] Connection released back to pool, current size: {}", _pool.size());
	_condition.notify_one();
}

MySQLConPool::~MySQLConPool()
{
    LOG_INFO("[MySQLPool] Destroying connection pool");
	std::unique_lock<std::mutex> lock(_mutex);
	while (!_pool.empty()) {
		_pool.pop();
	}
    LOG_INFO("[MySQLPool] All connections cleared");
}

MySQLConPool::MySQLConPool():
	_b_stop(false),_poolSize(std::thread::hardware_concurrency())
{
    try {
        auto& configManager = ConfigManager::GetInstance();
        _host = configManager["MySQL"]["host"];
        _port = std::stoi(configManager["MySQL"]["port"]);
        _user = configManager["MySQL"]["user"];
        _password = configManager["MySQL"]["password"];
        _schema = configManager["MySQL"]["schema"];

        LOG_INFO("[MySQLPool] Initializing pool with {} connections", _poolSize);
        LOG_DEBUG("[MySQLPool] Connection parameters - Host: {}, Port: {}, Schema: {}, User: {}",
            _host, _port, _schema, _user);

        for (std::size_t i = 0; i < _poolSize; ++i) {
            auto conn = CreateConnection();
            if (conn) {
                _pool.push(std::move(conn));
                LOG_DEBUG("[MySQLPool] Created connection {}/{}", i + 1, _poolSize);
            }
            else {
                LOG_ERROR("[MySQLPool] Failed to create connection {}/{}", i + 1, _poolSize);
            }
        }

        LOG_INFO("[MySQLPool] Pool initialization completed, active connections: {}", _pool.size());
    }
    catch (const mysqlx::Error& error) {
        LOG_CRITICAL("[MySQLPool] Pool initialization failed: {}", error.what());
        throw;
    }
}

std::unique_ptr<mysqlx::Session> MySQLConPool::CreateConnection()
{
    try {
        std::string connStr = "mysqlx://" + _user + ":" + _password +
            "@" + _host + ":" + std::to_string(_port) +
            "/" + _schema;
        LOG_DEBUG("[MySQLPool] Attempting to create new connection");

        auto session = std::make_unique<mysqlx::Session>(connStr);
        LOG_DEBUG("[MySQLPool] Successfully created new connection");
        return session;
    }
    catch (const mysqlx::Error& error) {
        LOG_ERROR("[MySQLPool] Failed to create connection: {}", error.what());
        return nullptr;
    }
} 