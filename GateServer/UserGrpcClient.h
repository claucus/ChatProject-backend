#pragma once
#include <grpcpp/grpcpp.h>
#include "user.grpc.pb.h"
#include "Singleton.h"
#include "GrpcPool.h"
#include "const.h"


class UserGrpcClient : public Singleton<UserGrpcClient>
{
	using UserConPool = GrpcPool<user::UserService, user::UserService::Stub>;
	friend class Singleton<UserGrpcClient>;

public:
	user::VerifyEmailCodeResp VerifyEmailCode(const std::string& email, const std::string& code);
	user::RegisterUserResp RegisterUser(const user::RegisterUserReq& req);
	user::VerifyLoginResp VerifyLogin(const user::VerifyLoginReq& req);
	user::ResetPasswordResp ResetPassword(const user::ResetPasswordReq& req);
	user::GetUserProfileResp GetUserProfile(const user::GetUserProfileReq& req);

private:
	UserGrpcClient();
	std::unique_ptr<UserConPool> _pool;
};

