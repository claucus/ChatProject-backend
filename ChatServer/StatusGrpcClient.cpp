#include "StatusGrpcClient.h"
#include "ConfigManager.h"
#include "Logger.h"

StatusGrpcClient::StatusGrpcClient()
{
	auto& cfg = ConfigManager::GetInstance();
	std::string host = cfg["StatusServer"]["host"];
	std::string port = cfg["StatusServer"]["port"];
	LOG_INFO("StatusGrpcClient init {}:{}", host, port);
	_pool.reset(new StatusPool(std::thread::hardware_concurrency(), host, port));
}

status::OnlineReportResp StatusGrpcClient::ReportOnline(const std::string& uid, const std::string& serverName, const std::string& host, int port, const std::string& token)
{
	grpc::ClientContext ctx;
	status::OnlineReportReq req;
	status::OnlineReportResp resp;

	req.set_uid(uid);
	req.set_server_name(serverName);
	req.set_server_host(host);
	req.set_server_port(port);
	req.set_token(token);

	auto stub = _pool->GetConnection();
	auto status = stub->ReportOnline(&ctx, req, &resp);
	_pool->ReturnConnection(std::move(stub));
	if (!status.ok()) {
		LOG_ERROR("ReportOnline RPC failed: {}", status.error_message());
		resp.set_error(1);
	}
	return resp;
}

status::OfflineReportResp StatusGrpcClient::ReportOffline(const std::string& uid)
{
	grpc::ClientContext ctx;
	status::OfflineReportReq req;
	status::OfflineReportResp resp;

	req.set_uid(uid);

	auto stub = _pool->GetConnection();
	auto status = stub->ReportOffline(&ctx, req, &resp);
	_pool->ReturnConnection(std::move(stub));
	if (!status.ok()) {
		LOG_ERROR("ReportOffline RPC failed: {}", status.error_message());
		resp.set_error(1);
	}
	return resp;
}

status::RouteResp StatusGrpcClient::QueryUserRoute(const std::string& uid)
{
	grpc::ClientContext ctx;
	status::RouteReq req;
	status::RouteResp resp;

	req.set_uid(uid);

	auto stub = _pool->GetConnection();
	auto status = stub->QueryUserRoute(&ctx, req, &resp);
	_pool->ReturnConnection(std::move(stub));
	if (!status.ok()) {
		LOG_ERROR("QueryUserRoute RPC failed: {}", status.error_message());
		resp.set_error(1);
		resp.set_online(false);
	}
	return resp;
}

status::RegisterNodeResp StatusGrpcClient::RegisterNode(const std::string& name, const std::string& host, int port, int capacity)
{
	grpc::ClientContext ctx;
	status::RegisterNodeReq req;
	status::RegisterNodeResp resp;

	req.set_name(name);
	req.set_server_host(host);
	req.set_server_port(port);
	req.set_capacity(capacity);

	auto stub = _pool->GetConnection();
	auto status = stub->RegisterNode(&ctx, req, &resp);
	_pool->ReturnConnection(std::move(stub));
	if (!status.ok()) {
		LOG_ERROR("RegisterNode RPC failed: {}", status.error_message());
		resp.set_error(1);
	}
	return resp;
}

status::HeartbeatResp StatusGrpcClient::Heartbeat(const std::string& name, const std::string& host, int port)
{
	grpc::ClientContext ctx;
	status::HeartbeatReq req;
	status::HeartbeatResp resp;

	req.set_name(name);
	req.set_server_host(host);
	req.set_server_port(port);

	auto stub = _pool->GetConnection();
	auto status = stub->Heartbeat(&ctx, req, &resp);
	_pool->ReturnConnection(std::move(stub));
	if (!status.ok()) {
		LOG_ERROR("Heartbeat RPC failed: {}", status.error_message());
		resp.set_error(1);
	}
	return resp;
}

status::KickUserResp StatusGrpcClient::KickUser(const std::string& uid, int reason)
{
	grpc::ClientContext ctx;
	status::KickUserReq req;
	status::KickUserResp resp;

	req.set_uid(uid);
	req.set_reason(reason);

	auto stub = _pool->GetConnection();
	auto status = stub->KickUser(&ctx, req, &resp);
	_pool->ReturnConnection(std::move(stub));
	if (!status.ok()) {
		LOG_ERROR("KickUser RPC failed: {}", status.error_message());
		resp.set_error(1);
	}
	return resp;
}