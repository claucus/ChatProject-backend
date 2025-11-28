#pragma once
#include <grpcpp/grpcpp.h>
#include "user.grpc.pb.h"

class UserServerImpl final: public user::UserService::Service
{
public:
	UserServerImpl();

	grpc::Status VerifyEmailCode(grpc::ServerContext* context, const user::VerifyEmailCodeReq* request, user::VerifyEmailCodeResp* response) override;
	grpc::Status RegisterUser(grpc::ServerContext* context, const user::RegisterUserReq* request, user::RegisterUserResp* response) override;
	grpc::Status VerifyLogin(grpc::ServerContext* context, const user::VerifyLoginReq* request, user::VerifyLoginResp* response) override;
	grpc::Status ResetPassword(grpc::ServerContext* context, const user::ResetPasswordReq* request, user::ResetPasswordResp* response) override;
	grpc::Status GetUserProfile(grpc::ServerContext* context, const user::GetUserProfileReq* request, user::GetUserProfileResp* response) override;

private:
	bool RegisterUserDB(const user::RegisterUserReq& req, std::string& outUid, int& outError);
	bool VerifyLoginDB(const user::VerifyLoginReq& req, int& outError, std::string& outUid);
	bool ResetPasswordDB(const user::ResetPasswordReq& req, int& outError);
	bool GetUserProfileDB(const std::string& uid, user::GetUserProfileResp* resp);
};

