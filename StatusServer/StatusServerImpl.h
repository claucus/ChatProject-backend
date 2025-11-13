#pragma once
#include <grpcpp/grpcpp.h>
#include "message.grpc.pb.h"
#include <unordered_map>
#include <mutex>

struct ChatServer {
	std::string host;
	std::string port;
	std::string name;
	size_t con_count;

	ChatServer(): host(""),port(""),name(""),con_count(0){}
	ChatServer(const ChatServer& server): host(server.host),port(server.port),name(server.name),con_count(server.con_count){}
	ChatServer& operator=(const ChatServer& server) {
		if (&server == this) {
			return *this;
		}

		host = server.host;
		port = server.port;
		name = server.name;
		con_count = server.con_count;
		
		return *this;
	}
};

class StatusServerImpl final : public message::StatusService::Service
{
public:
	StatusServerImpl();

	grpc::Status GetChatServer(
		grpc::ServerContext * context,
		const message::GetChatServerRequest * request,
		message::GetChatServerResponse * response) override;


	grpc::Status Login(
		grpc::ServerContext * context,
		const message::LoginRequest *request,
		message::LoginResponse * response) override;

private:
	ChatServer GetChatServer();
	void saveSessionToRedis(const std::string& uid, const std::string& serverName, const std::string& token);
	std::optional<std::tuple<std::string, std::string, std::string>> getSessionFromRedis(const std::string& uid);
private:
	std::unordered_map <std::string, ChatServer> _servers;
	std::mutex _serverMutex;
	std::mutex _tokenMutex;
};

