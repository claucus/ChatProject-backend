#include "RedisConPool.h"
#include "ConfigManager.h"
#include <spdlog/spdlog.h>

sw::redis::Redis& RedisConPool::GetInstance()
{
    static RedisConPool con;
    return *con._redis;
}

RedisConPool::~RedisConPool()
{
    spdlog::info("[RedisPool] Destroying Redis connection pool");
}

RedisConPool::RedisConPool()
{
    spdlog::info("[RedisPool] Initializing Redis connection pool");
    
    try {
        sw::redis::ConnectionOptions _options;
        auto& configManager = ConfigManager::GetInstance();
        
        _options.host = configManager["Redis"]["host"];
        _options.port = std::stoi(configManager["Redis"]["port"]);
        _options.socket_timeout = std::chrono::milliseconds(200);
        
        spdlog::debug("[RedisPool] Configuring connection - Host: {}, Port: {}, Timeout: {}ms", 
                      _options.host, _options.port, 200);

        _redis = std::make_unique<sw::redis::Redis>(_options);
        
        // 验证连接是否成功
        try {
            _redis->ping();
            spdlog::info("[RedisPool] Successfully connected to Redis server");
        }
        catch (const sw::redis::Error& e) {
            spdlog::critical("[RedisPool] Failed to connect to Redis server: {}", e.what());
            throw; // 重新抛出异常，因为这是致命错误
        }
    }
    catch (const std::exception& e) {
        spdlog::critical("[RedisPool] Redis pool initialization failed: {}", e.what());
        throw; // 重新抛出异常，表示初始化失败
    }
}

void RedisConPool::HealthCheck()
{
    try {
        if (_redis) {
            _redis->ping();
            spdlog::debug("[RedisPool] Health check successful");
        }
        else {
            spdlog::error("[RedisPool] Redis connection is null");
        }
    }
    catch (const sw::redis::Error& e) {
        spdlog::error("[RedisPool] Health check failed: {}", e.what());
    }
}
