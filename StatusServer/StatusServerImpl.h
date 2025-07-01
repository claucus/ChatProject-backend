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
	void insertToken(std::string uuid,std::string token);
	ChatServer GetChatServer();

private:
	std::unordered_map <std::string, ChatServer> _servers;
	std::mutex _serverMutex;

	std::unordered_map<std::string, std::string> _tokens;
	std::mutex _tokenMutex;
};

