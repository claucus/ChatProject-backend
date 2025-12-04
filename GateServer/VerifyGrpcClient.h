#pragma once
#include <grpcpp/grpcpp.h>
#include "verify.grpc.pb.h"
#include "Singleton.h"
#include "GrpcPool.h"
#include "const.h"

class VerifyGrpcClient :public Singleton<VerifyGrpcClient>
{
	using VerifyConPool = GrpcPool<verify::VerifyService, verify::VerifyService::Stub>;
	friend class Singleton<VerifyGrpcClient>;
public:
	verify::GetVerifyResp GetVerifyCode(std::string email);

private:
	VerifyGrpcClient();
	std::unique_ptr<VerifyConPool> _pool;
};

