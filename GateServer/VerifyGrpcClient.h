#pragma once
#include <grpcpp/grpcpp.h>
#include "message.grpc.pb.h"
#include "Singleton.h"
#include "GrpcPool.h"
#include "const.h"

class VerifyGrpcClient :public Singleton<VerifyGrpcClient>
{
	using VerifyConPool = GrpcPool<message::VerifyService, message::VerifyService::Stub>;
	friend class Singleton<VerifyGrpcClient>;
public:
	message::GetVerifyResponse GetVerifyCode(std::string email);

private:
	VerifyGrpcClient();
	std::unique_ptr<VerifyConPool> _pool;
};

