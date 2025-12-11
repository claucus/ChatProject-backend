#pragma once
#include <memory>
#include <grpcpp/grpcpp.h>
#include "status.grpc.pb.h"
#include "Singleton.h"
#include "GrpcPool.h"

#include "const.h"

/**
 * @class StatusGrpcClient
 * @brief 
 * a gRPC client, and now it just for allocating server when user login.
 * It follows the Singleton pattern.
 */
class StatusGrpcClient : public Singleton<StatusGrpcClient> 
{
	using StatusPool = GrpcPool<status::StatusService, status::StatusService::Stub>;
	friend class Singleton<StatusGrpcClient>;

public:
	/**
	 * @brief Allocate a server for a user when user login.
	 * 
	 * @param uid 
	 * @return status::AllocateServerResp
	 * it returns AllocateServerResp which contains server info.
	 */
	status::AllocateServerResp AllocateServer(const std::string& uid);

private:
	/**
	 * @brief Construct a new Status Grpc Client:: Status Grpc Client object
	 * 
	 */
	StatusGrpcClient();
	std::unique_ptr<StatusPool> _pool; 
};
