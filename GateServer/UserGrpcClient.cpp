#include "UserGrpcClient.h"

#include "ConfigManager.h"
#include "Logger.h"

user::VerifyEmailCodeResp UserGrpcClient::VerifyEmailCode(const std::string& email, const std::string& code)
{
	grpc::ClientContext context;
	user::VerifyEmailCodeReq req;
	user::VerifyEmailCodeResp resp;
	req.set_email(email);
	req.set_code(code);

	auto stub = _pool->GetConnection();
	LOG_INFO("UserGrpcClient: VerifyEmailCode email is : {}", email);

	auto status = stub->VerifyEmailCode(&context, req, &resp);
	if (!status.ok()) {
		LOG_ERROR("gRPC VerifyEmailCode failed : {}", status.error_message());
		resp.set_ok(false);
		resp.set_error(static_cast<int>(ErrorCodes::RPC_FAILED));
	}
	else {
		LOG_DEBUG("gRPC VerifyEmailCode succeeded for email = {}", email);
	}
	_pool->ReturnConnection(std::move(stub));
	return resp;
}

user::RegisterUserResp UserGrpcClient::RegisterUser(const user::RegisterUserReq& req)
{
	grpc::ClientContext context;
	user::RegisterUserResp resp;

	auto stub = _pool->GetConnection();
	LOG_INFO("UserGrpcClient: RegisterUser email is : {}", req.email());

	auto status = stub->RegisterUser(&context, req, &resp);
	if (!status.ok()) {
		LOG_ERROR("gRPC RegisterUser failed : {}", status.error_message());
		resp.set_ok(false);
		resp.set_error(static_cast<int>(ErrorCodes::RPC_FAILED));
	}
	else {
		LOG_DEBUG("gRPC RegisterUser succeeded for email = {}", req.email());
	}
	_pool->ReturnConnection(std::move(stub));
	return resp;
}

user::VerifyLoginResp UserGrpcClient::VerifyLogin(const user::VerifyLoginReq& req)
{
	grpc::ClientContext context;
	user::VerifyLoginResp resp;

	auto stub = _pool->GetConnection();
	LOG_INFO("UserGrpcClient: VerifyLogin email is : {}", req.email().empty()?"<empty>":req.email());
	LOG_INFO("UserGrpcClient: VerifyLogin uid is : {}", req.uid().empty()?"<empty>":req.uid());

	auto status = stub->VerifyLogin(&context, req, &resp);
	if (!status.ok()) {
		LOG_ERROR("gRPC VerifyLogin failed : {}", status.error_message());
		resp.set_ok(false);
		resp.set_error(static_cast<int>(ErrorCodes::RPC_FAILED));
	}
	else {
		LOG_DEBUG("gRPC VerifyLogin succeeded for user: {}", req.email().empty()?req.uid():req.email());
	}
	_pool->ReturnConnection(std::move(stub));
	return resp;
}

user::ResetPasswordResp UserGrpcClient::ResetPassword(const user::ResetPasswordReq& req)
{
	grpc::ClientContext context;
	user::ResetPasswordResp resp;

	auto stub = _pool->GetConnection();
	LOG_INFO("UserGrpcClient: ResetPassword email is : {}", req.email().empty() ? "<empty>" : req.email());
	LOG_INFO("UserGrpcClient: ResetPassword uid is : {}", req.uid().empty() ? "<empty>" : req.uid());

	auto status = stub->ResetPassword(&context, req, &resp);
	if (!status.ok()) {
		LOG_ERROR("gRPC ResetPassword failed : {}", status.error_message());
		resp.set_ok(false);
		resp.set_error(static_cast<int>(ErrorCodes::RPC_FAILED));
	}
	else {
		LOG_DEBUG("gRPC ResetPassword succeeded for uid = {}", req.uid());
	}
	_pool->ReturnConnection(std::move(stub));
	return resp;
}

user::GetUserProfileResp UserGrpcClient::GetUserProfile(const user::GetUserProfileReq& req)
{
	grpc::ClientContext context;
	user::GetUserProfileResp resp;

	auto stub = _pool->GetConnection();
	LOG_INFO("UserGrpcClient: GetUserProfile uid is : {}", req.uid().empty() ? "<empty>" : req.uid());

	auto status = stub->GetUserProfile(&context, req, &resp);
	if (!status.ok()) {
		LOG_ERROR("gRPC GetUserProfile failed : {}", status.error_message());
		resp.set_ok(false);
	}
	else {
		LOG_DEBUG("gRPC GetUserProfile succeeded for uid = {}", req.uid());
	}
	_pool->ReturnConnection(std::move(stub));
	return resp;
}

UserGrpcClient::UserGrpcClient()
{
	auto& cfg = ConfigManager::GetInstance();
	std::string host = cfg["UserServer"]["host"];
	std::string port = cfg["UserServer"]["port"];

	LOG_INFO("Initializing UserGrpcClient, host: {}, port: {}", host, port);

	_pool.reset(new UserConPool((size_t)(std::thread::hardware_concurrency()), host, port));
}
