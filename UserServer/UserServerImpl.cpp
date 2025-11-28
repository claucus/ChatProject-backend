#include "UserServerImpl.h"

#include "RedisConPool.h"
#include "ConfigManager.h"
#include "Logger.h"
#include "MySQLManager.h"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

UserServerImpl::UserServerImpl()
{
	LOG_INFO("Starting UserServerImpl");
}

grpc::Status UserServerImpl::VerifyEmailCode(grpc::ServerContext* context, const user::VerifyEmailCodeReq* request, user::VerifyEmailCodeResp* response)
{
	LOG_INFO("VerifyEmailCode email: {}", request->email());

	auto stored = RedisConPool::GetInstance().get(CODE_PREFIX + request->email()).value();
	if (stored.empty()) {
		LOG_WARN("Verify code timeout for email: {}", request->email());
		response->set_ok(false);
		response->set_error(static_cast<int>(RegisterResponseCodes::VERIFY_CODE_TIMEOUT));
		return grpc::Status::OK;
	}
	
	if (stored != request->code()) {
		LOG_WARN("Verify code mismatch for email: {}", request->email());
		response->set_ok(false);
		response->set_error(static_cast<int>(RegisterResponseCodes::VERIFY_CODE_ERROR));
		return grpc::Status::OK;
	}
	
	response->set_ok(true);
	response->set_error(static_cast<int>(ErrorCodes::SUCCESS));
	return grpc::Status::OK;
}

grpc::Status UserServerImpl::RegisterUser(grpc::ServerContext* context, const user::RegisterUserReq* request, user::RegisterUserResp* response)
{
	LOG_INFO("RegisterUser email: {}", request->email());

	user::VerifyEmailCodeReq verifyReq;
	verifyReq.set_email(request->email());
	verifyReq.set_code(request->verify_code());

	user::VerifyEmailCodeResp verifyResp;
	VerifyEmailCode(nullptr, &verifyReq, &verifyResp);
	if (!verifyResp.ok()) {
		response->set_ok(false);
		response->set_error(verifyResp.error());
		return grpc::Status::OK;
	}

	std::string outUid;
	int outError = 0;
	bool ok = RegisterUserDB(*request, outUid, outError);
	if (!ok) {
		LOG_WARN("User already exists: {}", request->email());
		response->set_ok(false);
		response->set_error(outError);
		return grpc::Status::OK;
	}

	LOG_INFO("User registered successfully: {}", request->email());

	response->set_ok(true);
	response->set_error(static_cast<int>(RegisterResponseCodes::REGISTER_SUCCESS));
	response->set_uid(outUid);
	response->set_email(request->email());
	response->set_username(request->username());
	return grpc::Status::OK;
}

grpc::Status UserServerImpl::VerifyLogin(grpc::ServerContext* context, const user::VerifyLoginReq* request, user::VerifyLoginResp* response)
{
	LOG_INFO("VerifyLogin email: {}", request->email().empty() ? "<empty>" : request->email());
	LOG_INFO("VerifyLogin uid: {}", request->uid().empty() ? "<empty>" : request->uid());

	int outError = 0;
	std::string outUid;
	if (!VerifyLoginDB(*request, outError, outUid)) {
		response->set_ok(false);
		response->set_error(outError);
		return grpc::Status::OK;
	}

	response->set_ok(true);
	response->set_error(static_cast<int>(LoginResponseCodes::LOGIN_SUCCESS));
	response->set_uid(outUid);
	response->set_email(request->email());
	return grpc::Status::OK;
}

grpc::Status UserServerImpl::ResetPassword(grpc::ServerContext* context, const user::ResetPasswordReq* request, user::ResetPasswordResp* response)
{
	LOG_INFO("ResetPassword email: {}", request->email().empty() ? "<empty>" : request->email());
	LOG_INFO("ResetPassword uid: {}", request->uid().empty()?"<empty>":request->uid());

	user::VerifyEmailCodeReq verifyReq;
	verifyReq.set_email(request->email());
	verifyReq.set_code(request->verify_code());

	user::VerifyEmailCodeResp verifyResp;
	VerifyEmailCode(nullptr, &verifyReq, &verifyResp);
	if (!verifyResp.ok()) {
		response->set_ok(false);
		response->set_error(verifyResp.error());
		return grpc::Status::OK;
	}

	int outError = 0;
	if (!ResetPasswordDB(*request, outError)) {
		LOG_WARN("User not exists for reset: {}", request->email().empty()?request->uid():request->email());
		response->set_ok(false);
		response->set_error(outError);
		return grpc::Status::OK;
	}

	LOG_INFO("Password reset successful for user: {}", request->email().empty() ? request->uid() : request->email());
	response->set_ok(true);
	response->set_error(static_cast<int>(ResetResponseCodes::RESET_SUCCESS));
	return grpc::Status::OK;
}

grpc::Status UserServerImpl::GetUserProfile(grpc::ServerContext* context, const user::GetUserProfileReq* request, user::GetUserProfileResp* response)
{
	LOG_INFO("GetUserProfile uid: {}", request->uid());
	if (!GetUserProfileDB(request->uid(), response)) {
		response->set_ok(false);
		return grpc::Status::OK;
	}
	response->set_ok(true);
	return grpc::Status::OK;
}

bool UserServerImpl::RegisterUserDB(const user::RegisterUserReq& req, std::string& outUid, int& outError)
{
	UserInfo info;
	
	info.email = req.email();
	info.username = req.username();
	info.password = req.password();

	bool ok = MySQLManager::GetInstance()->RegisterUser(info);
	if (!ok) {
		outError = static_cast<int>(RegisterResponseCodes::USER_EXISTS);
		return false;
	}

	outUid = info.uid;
	outError = static_cast<int>(RegisterResponseCodes::REGISTER_SUCCESS);
	return true;
}

bool UserServerImpl::VerifyLoginDB(const user::VerifyLoginReq& req, int& outError, std::string& outUid)
{
	UserInfo user;
	user.uid = req.uid();
	user.email = req.email();
	user.password = req.password();

	bool ok = MySQLManager::GetInstance()->UserLogin(user);
	if (!ok) {
		outError = static_cast<int>(LoginResponseCodes::LOGIN_ERROR);
		return false;
	}
	outUid = user.uid;
	outError = static_cast<int>(LoginResponseCodes::LOGIN_SUCCESS);
	return true;
}

bool UserServerImpl::ResetPasswordDB(const user::ResetPasswordReq& req, int& outError)
{
	UserInfo user;
	user.uid = req.uid();
	user.email = req.email();
	user.password = req.new_password();

	bool ok = MySQLManager::GetInstance()->ResetPassword(user);
	
	if (!ok) {
		outError = static_cast<int>(ErrorCodes::UID_INVALID);
		return false;
	}
	outError = static_cast<int>(ResetResponseCodes::RESET_SUCCESS);
	return true;
}

bool UserServerImpl::GetUserProfileDB(const std::string& uid, user::GetUserProfileResp* resp)
{
	auto info = MySQLManager::GetInstance()->GetUser(uid);
	if (!info) {
		return false;
	}

	resp->set_uid(info->uid);
	resp->set_email(info->email);
	resp->set_username(info->username);
	if (!info->birth.empty()) {
		resp->set_birth(info->birth);
	}
	if (!info->sex.empty()) {
		resp->set_sex(info->sex);
	}
	if (!info->avatar.empty()) {
		resp->set_avatar(info->avatar);
	}
	return true;
}
