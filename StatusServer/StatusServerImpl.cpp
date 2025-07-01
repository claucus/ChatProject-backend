#include "StatusServerImpl.h"
#include "ConfigManager.h"
#include "const.h"
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/random_generator.hpp>
#include <spdlog/spdlog.h>

std::string generateToken() {
    boost::uuids::uuid uid = boost::uuids::random_generator()();
    auto token = boost::uuids::to_string(uid);
    spdlog::debug("[StatusServer] Generated new token: {}", token);
    return token;
}

StatusServerImpl::StatusServerImpl()
{
    spdlog::info("[StatusServer] Initializing Status Server");
    auto& cfg = ConfigManager::GetInstance();
    
    try {
        // 初始化第一个聊天服务器
        ChatServer server;
        server.port = cfg["ChatServer1"]["port"];
        server.host = cfg["ChatServer1"]["host"];
        server.name = cfg["ChatServer1"]["name"];
        server.con_count = 0;
        _servers[server.name] = server;
        spdlog::info("[StatusServer] Registered Chat Server 1 - Name: {}, Host: {}:{}", 
                     server.name, server.host, server.port);

        // 初始化第二个聊天服务器
        server.port = cfg["ChatServer2"]["port"];
        server.host = cfg["ChatServer2"]["host"];
        server.name = cfg["ChatServer2"]["name"];
        server.con_count = 0;
        _servers[server.name] = server;
        spdlog::info("[StatusServer] Registered Chat Server 2 - Name: {}, Host: {}:{}", 
                     server.name, server.host, server.port);

        spdlog::info("[StatusServer] Initialization completed. Total servers: {}", _servers.size());
    }
    catch (const std::exception& e) {
        spdlog::critical("[StatusServer] Initialization failed: {}", e.what());
        throw;
    }
}

void StatusServerImpl::insertToken(std::string uuid, std::string token)
{
    std::lock_guard <std::mutex> lock(_tokenMutex);
    _tokens[uuid] = token;
    spdlog::debug("[StatusServer] Token inserted for UID: {}", uuid);
}

grpc::Status StatusServerImpl::GetChatServer(grpc::ServerContext* context, 
    const message::GetChatServerRequest* request, message::GetChatServerResponse* response)
{
    spdlog::info("[StatusServer] Received GetChatServer request from UID: {}", request->uid());
    
    const auto& server = GetChatServer();
    
    // 设置响应
    response->set_host(server.host);
    response->set_port(server.port);
    response->set_error(static_cast<int>(ErrorCodes::SUCCESS));
    response->set_token(generateToken());
    insertToken(request->uid(), response->token());

    spdlog::info("[StatusServer] Assigned server to client - Server: {}, Host: {}:{}, Connections: {}", 
                 server.name, server.host, server.port, server.con_count);
    spdlog::debug("[StatusServer] Generated token for UID {}: {}", request->uid(), response->token());

    return grpc::Status::OK;
}

ChatServer StatusServerImpl::GetChatServer()
{
    std::lock_guard<std::mutex> lock(_serverMutex);
    auto minServer = _servers.begin()->second;

    for (const auto& server : _servers) {
        if (server.second.con_count < minServer.con_count) {
            minServer = server.second;
        }
    }

    spdlog::debug("[StatusServer] Selected chat server {} with {} connections", 
                  minServer.name, minServer.con_count);
    return minServer;
}

grpc::Status StatusServerImpl::Login(grpc::ServerContext* context, 
    const message::LoginRequest* request, message::LoginResponse* response)
{
    spdlog::info("[StatusServer] Received login request - UID: {}", request->uid());
    spdlog::debug("[StatusServer] Login attempt with token: {}", request->token());

    auto uuid = request->uid();
    auto token = request->token();

    std::lock_guard<std::mutex> lock(_tokenMutex);

    auto iter = _tokens.find(uuid);
    if (iter == _tokens.end()) {
        spdlog::warn("[StatusServer] Login failed - Invalid UID: {}", uuid);
        response->set_error(static_cast<int>(ErrorCodes::UID_INVALID));
    }
    else if (iter->second != token) {
        spdlog::warn("[StatusServer] Login failed - Invalid token for UID: {}", uuid);
        response->set_error(static_cast<int>(ErrorCodes::TOKEN_INVALID));
    }
    else {
        spdlog::info("[StatusServer] Login successful for UID: {}", uuid);
        response->set_error(static_cast<int>(ErrorCodes::SUCCESS));
    }

    response->set_uid(uuid);
    response->set_token(token);
    
    return grpc::Status::OK;
}
