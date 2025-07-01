#pragma once
#include <memory>
#include <sw/redis++/redis++.h>

class RedisConPool
{
public:
	static sw::redis::Redis& GetInstance();
	~RedisConPool();
	RedisConPool(const RedisConPool&) = delete;
	RedisConPool& operator=(const RedisConPool&) = delete;
    void HealthCheck();

private:
	RedisConPool();

	std::unique_ptr<sw::redis::Redis> _redis;
};

