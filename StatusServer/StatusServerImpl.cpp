#include "StatusServerImpl.h"
#include "ConfigManager.h"
#include "const.h"
#include "RedisConPool.h"
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/random_generator.hpp>
#include "Logger.h"

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

void StatusServerImpl::insertToken(std::string uid, std::string token)
{
    std::lock_guard <std::mutex> lock(_tokenMutex);
    RedisConPool::GetInstance().set(ChatServiceConstant::USER_TOKEN_PREFIX + uid, token);
    LOG_DEBUG("Token inserted for UID: {}", uid);
}

grpc::Status StatusServerImpl::GetChatServer(grpc::ServerContext* context, 
    const message::GetChatServerRequest* request, message::GetChatServerResponse* response)
{
    LOG_INFO("Received GetChatServer request from UID: {}", request->uid());
    
    const auto& server = GetChatServer();
    
    // 设置响应
    response->set_host(server.host);
    response->set_port(server.port);
    response->set_error(static_cast<int>(ErrorCodes::SUCCESS));
    response->set_token(generateToken());
    insertToken(request->uid(), response->token());

    LOG_INFO("Assigned server to client - Server: {}, Host: {}:{}, Connections: {}", server.name, server.host, server.port, server.con_count);
    LOG_DEBUG("Generated token for UID {}: {}", request->uid(), response->token());

    return grpc::Status::OK;
}

ChatServer StatusServerImpl::GetChatServer()
{
    std::lock_guard<std::mutex> lock(_serverMutex);
    auto minServer = _servers.begin()->second;

    auto count = RedisConPool::GetInstance().hget(ChatServiceConstant::LOGIN_COUNT, minServer.name).value();
    if (count.empty()) {
        minServer.con_count = INT_MAX;
    }
    else {
		minServer.con_count = std::stoi(count);
    }


    for (auto& server : _servers) {

        if (server.second.name == minServer.name) {
            continue;
        }

		auto count = RedisConPool::GetInstance().hget(ChatServiceConstant::LOGIN_COUNT, server.second.name).value();
        if (count.empty()) {
            server.second.con_count = INT_MAX;
        }
        else {
			server.second.con_count = std::stoi(count);
        }

        if (server.second.con_count < minServer.con_count) {
            minServer = server.second;
        }
    }

    LOG_DEBUG("Selected chat server {} with {} connections", minServer.name, minServer.con_count);
    return minServer;
}

grpc::Status StatusServerImpl::Login(grpc::ServerContext* context, 
    const message::LoginRequest* request, message::LoginResponse* response)
{
    LOG_INFO("Received login request - UID: {}", request->uid());
    LOG_DEBUG("Login attempt with token: {}", request->token());

    auto uid = request->uid();
    auto token = request->token();

    std::lock_guard<std::mutex> lock(_tokenMutex);

    auto token_value = RedisConPool::GetInstance().get(ChatServiceConstant::USER_TOKEN_PREFIX + uid).value();

    if (token_value.empty()) {
        response->set_error(static_cast<int>(ErrorCodes::UID_INVALID));
        LOG_WARN("Login failed - Invalid UID: {}", uid);
        return grpc::Status::OK;
    }

    if (token_value != token) {
        response->set_error(static_cast<int>(ErrorCodes::TOKEN_INVALID));
		LOG_WARN("Login failed - Invalid token for UID: {}", uid);
        return grpc::Status::OK;
    }

    LOG_INFO("Login successful for UID: {}", uid);

    response->set_error(static_cast<int>(ErrorCodes::SUCCESS));
    response->set_uid(uid);
    response->set_token(token);
    
    return grpc::Status::OK;
}
