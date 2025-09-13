#include "RedisConPool.h"
#include "ConfigManager.h"
#include "Logger.h"

sw::redis::Redis& RedisConPool::GetInstance()
{
	static RedisConPool con;
	return *con._redis;
}

RedisConPool::~RedisConPool()
{
	LOG_INFO("Destroying Redis connection pool");
}

RedisConPool::RedisConPool()
{
	LOG_INFO("Initializing Redis connection pool");

	try {
		sw::redis::ConnectionOptions _options;
		auto& configManager = ConfigManager::GetInstance();

		_options.host = configManager["Redis"]["host"];
		_options.port = std::stoi(configManager["Redis"]["port"]);
		_options.socket_timeout = std::chrono::milliseconds(200);

		LOG_DEBUG("Configuring connection - Host: {}, Port: {}, Timeout: {}ms", _options.host, _options.port, 200);

		_redis = std::make_unique<sw::redis::Redis>(_options);

		try {
			_redis->ping();
			LOG_INFO("Successfully connected to Redis server");
		}
		catch (const sw::redis::Error& e) {
			LOG_CRITICAL("Failed to connect to Redis server: {}", e.what());
			throw;
		}
	}
	catch (const std::exception& e) {
		LOG_CRITICAL("Redis pool initialization failed: {}", e.what());
		throw;
	}
}

void RedisConPool::HealthCheck()
{
	try {
		if (_redis) {
			_redis->ping();
			LOG_DEBUG("Health check successful");
		}
		else {
			LOG_ERROR("Redis connection is null");
		}
	}
	catch (const sw::redis::Error& e) {
		LOG_ERROR("Health check failed: {}", e.what());
	}
}
