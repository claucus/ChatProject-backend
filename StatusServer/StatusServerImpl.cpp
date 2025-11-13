#include "StatusServerImpl.h"
#include "ConfigManager.h"
#include "const.h"
#include "RedisConPool.h"
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/random_generator.hpp>
#include "Logger.h"

#include <nlohmann/json.hpp>
using json = nlohmann::json;

std::string generateToken() {
    boost::uuids::uuid uid = boost::uuids::random_generator()();
    auto token = boost::uuids::to_string(uid);
    LOG_DEBUG("Generated new token: {}", token);
    return token;
}

StatusServerImpl::StatusServerImpl()
{
    LOG_INFO("Initializing Status Server");
    auto& cfg = ConfigManager::GetInstance();

    try {
        auto serverList = cfg["ServerList"]["ServerName"];
        
        LOG_INFO("ServerList Load Successful");

        std::vector<std::string> servers;

        std::stringstream ss(serverList);
        std::string server;

        while (std::getline(ss, server, ',')) {
            servers.emplace_back(server);
			LOG_DEBUG("Found server: {}", server);
        }

        size_t cnt = 0;
        for (auto& serverName : servers) {
            if (cfg[serverName]["name"].empty()) {
                continue;
            }

            ChatServer server;
            server.host = cfg[serverName]["host"];
            server.port = cfg[serverName]["port"];
            server.name = cfg[serverName]["name"];
            _servers[server.name] = server;
            
            LOG_INFO("Registered Chat Server {} - Name: {}, Host: {}:{}", ++cnt, server.name, server.host, server.port);
        }

        LOG_INFO("Initialization completed. Total servers: {}", _servers.size());
    }
    catch (const std::exception& e) {
        LOG_CRITICAL("Initialization failed: {}", e.what());
        throw;
    }
}

grpc::Status StatusServerImpl::GetChatServer(grpc::ServerContext* context, 
    const message::GetChatServerRequest* request, message::GetChatServerResponse* response)
{
    auto uid = request->uid();
    LOG_INFO("Received GetChatServer request from UID: {}", uid);
    
    auto sessionOpt = getSessionFromRedis(uid);
    if (sessionOpt) {
        const auto& [serverName, savedToken, times] = *sessionOpt;
        const auto& server = _servers[serverName];

        response->set_host(server.host);
		response->set_port(server.port);
		response->set_error(static_cast<int>(ErrorCodes::SUCCESS));
        response->set_token(savedToken);

		LOG_INFO("Existing session found for UID: {}. Assigned server: {}, Host: {}:{}, Connections: {}", uid, server.name, server.host, server.port, server.con_count);
		return grpc::Status::OK;
    }

    const auto& server = GetChatServer();
    std::string token = generateToken();
    saveSessionToRedis(uid, server.name, token);


    // 设置响应
    response->set_host(server.host);
    response->set_port(server.port);
    response->set_error(static_cast<int>(ErrorCodes::SUCCESS));
    response->set_token(token);

    LOG_INFO("Assigned server to client - Server: {}, Host: {}:{}, Connections: {}", server.name, server.host, server.port, server.con_count);
    LOG_DEBUG("Generated token for UID {}: {}", request->uid(), response->token());

    return grpc::Status::OK;
}

ChatServer StatusServerImpl::GetChatServer()
{
    std::lock_guard<std::mutex> lock(_serverMutex);

    ChatServer* minServer = nullptr;
    auto minCount = INT_MAX;

    for (auto& [name, server] : _servers) {
        auto cnt = RedisConPool::GetInstance().hget(ChatServiceConstant::LOGIN_COUNT,server.name).value();

        int count = cnt.empty() ? 0 : std::stoi(cnt);
        server.con_count = count;

        if (count < minCount) {
            minCount = count;
            minServer = &server;
        }
    }

    if (minServer) {
        LOG_DEBUG("Selected chat server {} with {} connections", minServer->name, minServer->con_count);
        return *minServer;
    }
    else {
        auto it = _servers.begin();
		LOG_WARN("No servers available, defaulting to first server: {}", it->second.name);
        return it->second;
    }
}

grpc::Status StatusServerImpl::Login(grpc::ServerContext* context, 
    const message::LoginRequest* request, message::LoginResponse* response)
{
    LOG_INFO("Received login request - UID: {}", request->uid());
    LOG_DEBUG("Login attempt with token: {}", request->token());

    auto uid = request->uid();
    auto token = request->token();

    std::lock_guard<std::mutex> lock(_tokenMutex);

    auto sessionOpt = getSessionFromRedis(uid);
    if (sessionOpt) {
		const auto& [serverName, savedToken, times] = *sessionOpt;

        if (savedToken.empty()) {
			response->set_error(static_cast<int>(ErrorCodes::UID_INVALID));
			LOG_WARN("Login failed - Invalid UID: {}", uid);
			return grpc::Status::OK;
        }
        
        if (token != savedToken) {
			response->set_error(static_cast<int>(ErrorCodes::TOKEN_INVALID));
			LOG_WARN("Login failed - Invalid token for UID: {}", uid);
			return grpc::Status::OK;
        }


		response->set_error(static_cast<int>(ErrorCodes::SUCCESS));
		response->set_uid(uid);
		response->set_token(savedToken);

		LOG_INFO("Login successful from existing session for UID: {}", uid);
		return grpc::Status::OK;
    }

    response->set_error(static_cast<int>(ErrorCodes::SUCCESS));
    response->set_uid(uid);
    response->set_token(token);

	LOG_INFO("Login successful for UID: {}", uid);
    
    return grpc::Status::OK;
}

void StatusServerImpl::saveSessionToRedis(const std::string& uid, const std::string& serverName, const std::string& token)
{
    json sessionJson = {
        {"server_name",serverName},
        {"token",token},
        {"times","0"}
    };

    std::string key = ChatServiceConstant::USER_SESSION_PREFIX + uid;
    RedisConPool::GetInstance().setex(key, SESSION_EXPIRE_SECONDS, sessionJson.dump());
    LOG_DEBUG("Session saved to Redis for UID: {}", uid);
}

std::optional<std::tuple<std::string, std::string, std::string>> StatusServerImpl::getSessionFromRedis(const std::string& uid)
{
	std::string key = ChatServiceConstant::USER_SESSION_PREFIX + uid;
    auto value = RedisConPool::GetInstance().get(key).value();
    
    if (value.empty()) {
		LOG_DEBUG("No session found in Redis for UID: {}", uid);
        return std::nullopt;
    }
    try {
		auto sessionJson = json::parse(value);
		return std::make_tuple(sessionJson["server_name"].get<std::string>(), sessionJson["token"].get<std::string>(),sessionJson["times"].get<std::string>());
    }
    catch (...) {
		LOG_WARN("Failed to parse session JSON for UID: {}", uid);
		return std::nullopt;
    }
}
