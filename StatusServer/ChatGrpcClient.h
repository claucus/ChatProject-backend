#pragma once
#include <grpcpp/grpcpp.h>
#include "chat.grpc.pb.h"
#include "Singleton.h"
#include "GrpcPool.h"


class ChatGrpcClient: public Singleton<ChatGrpcClient>
{
	using ChatPool = GrpcPool<chat::ChatService,chat::ChatService::Stub>;
	friend class Singleton<ChatGrpcClient>;

public:
	chat::KickUserResp KickUser(const std::string& host, int port, const std::string& uid, int reason);
private:
	ChatGrpcClient();

	ChatPool* initPool(const std::string& host, int port);

	std::unordered_map<std::string, std::unique_ptr<ChatPool> > _pools;
	std::mutex _poolMutex;
};

