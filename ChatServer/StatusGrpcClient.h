#pragma once
#include <memory>
#include <grpcpp/grpcpp.h>
#include "status.grpc.pb.h"
#include "Singleton.h"
#include "GrpcPool.h"

#include "const.h"

/**
 * @class StatusGrpcClient
 * @brief gRPC client for communicating with the Status Service.
 * It follows the Singleton pattern.
 */
class StatusGrpcClient : public Singleton<StatusGrpcClient>
{
	using StatusPool = GrpcPool<status::StatusService, status::StatusService::Stub>;
	friend class Singleton<StatusGrpcClient>;

public:
	/**
	 * @brief Report a user coming online
	 *
	 * @param uid
	 * @param serverName
	 * @param host
	 * @param port
	 * @param token
	 * @return status::OnlineReportResp
	 */
	status::OnlineReportResp ReportOnline(const std::string& uid, const std::string& serverName, const std::string& server_host, int server_port, const std::string& token);

	/**
	 * @brief Report a user going offline
	 *
	 * @param uid
	 * @return status::OfflineReportResp
	 */
	status::OfflineReportResp ReportOffline(const std::string& uid);

	/**
	 * @brief Query the route information for a user
	 *
	 * @param uid
	 * @return status::RouteResp
	 */
	status::RouteResp QueryUserRoute(const std::string& uid);

	/**
	 * @brief Register a new node with the status server
	 *
	 * @param name
	 * @param host
	 * @param port
	 * @param capacity
	 * @return status::RegisterNodeResp
	 */
	status::RegisterNodeResp RegisterNode(const std::string& name, const std::string& server_host, int server_port, int capacity = 10000);

	/**
	 * @brief Send a heartbeat signal to the status server
	 *
	 * @param name
	 * @param host
	 * @param port
	 * @return status::HeartbeatResp
	 */
	status::HeartbeatResp Heartbeat(const std::string& name, const std::string& server_host, int server_port);

	/**
	 * @brief Kick a user from the server
	 *
	 * @param uid
	 * @param reason
	 * @return status::KickUserResp
	 */
	status::KickUserResp KickUser(const std::string& uid, int reason);

private:
	/**
	 * @brief Construct a new Status Grpc Client:: Status Grpc Client object
	 *
	 */
	StatusGrpcClient();
	std::unique_ptr<StatusPool> _pool;
};
