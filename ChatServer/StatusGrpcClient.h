#pragma once
#include <grpcpp/grpcpp.h>
#include "message.grpc.pb.h"
#include "Singleton.h"
#include "GrpcPool.h"
#include "const.h"


class StatusGrpcClient:public Singleton<StatusGrpcClient>
{
	using StatusConPool = GrpcPool<message::StatusService, message::StatusService::Stub>;
	friend class Singleton<StatusGrpcClient>;
public:
	message::GetChatServerResponse GetChatServer(std::string uid);
	message::LoginResponse Login(std::string uid,std::string token);
private:
	StatusGrpcClient();
	std::unique_ptr<StatusConPool> _pool;
};

