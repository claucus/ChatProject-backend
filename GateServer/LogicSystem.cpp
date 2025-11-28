#include "const.h"
#include "HttpConnection.h"
#include "LogicSystem.h"
#include "VerifyGrpcClient.h"
#include "StatusGrpcClient.h"
#include "UserGrpcClient.h"

#include "Defer.h"
#include "Logger.h"

#include <nlohmann/json.hpp>
using json = nlohmann::json;


bool LogicSystem::HandleGet(std::string path, std::shared_ptr<HttpConnection> con)
{
	if (_getHandlers.find(path) == _getHandlers.end()) {
		LOG_WARN("GET handler not found for path: {}", path);
		return false;
	}
	LOG_INFO("Handling GET request for path: {}", path);
	_getHandlers[path](con);
	return true;
}

bool LogicSystem::HandlePost(std::string path, std::shared_ptr<HttpConnection> con)
{
	if (_postHandlers.find(path) == _postHandlers.end()) {
		LOG_WARN("POST handler not found for path: {}", path);
		return false;
	}
	LOG_INFO("Handling POST request for path: {}", path);
	_postHandlers[path](con);
	return true;
}

void LogicSystem::RegisterGet(std::string url, HttpHandler handler)
{
	LOG_DEBUG("Registering GET handler for url: {}", url);
	_getHandlers.insert(std::make_pair(url, handler));
}

void LogicSystem::RegisterPost(std::string url, HttpHandler handler)
{
	LOG_DEBUG("Registering POST handler for url: {}", url);
	_postHandlers.insert(std::make_pair(url, handler));
}

LogicSystem::LogicSystem()
{
	LOG_INFO("Initializing LogicSystem and registering HTTP handlers");

	RegisterPost(
		"/postVerifyCode",
		[](std::shared_ptr<HttpConnection> connection) {
			LOG_INFO("Handling /postVerifyCode POST request");
			auto bodyString = boost::beast::buffers_to_string(connection->_request.body().data());
			LOG_DEBUG("Received body: {}", bodyString);

			connection->_response.set(boost::beast::http::field::content_type, "text/json");

			json root;
			defer{
				std::string jsonString = root.dump(4);
				boost::beast::ostream(connection->_response.body()) << jsonString;
				return true;
			};

			try {
				auto src = json::parse(bodyString);

				std::string email = src["email"].get<std::string>();
				LOG_INFO("Requesting verify code for email: {}", email);
				auto response = VerifyGrpcClient::GetInstance()->GetVerifyCode(email);

				root["error"] = response.error();
				root["email"] = email;
				root["verify_code"] = response.code();
			}
			catch (const json::parse_error& e) {
				LOG_WARN("Failed to parse JSON in /postVerifyCode: {}", e.what());
				root["error"] = static_cast<int>(ErrorCodes::ERROR_JSON);
			}
		}
	);

	RegisterPost(
		"/userRegister",
		[](std::shared_ptr<HttpConnection> connection) {
			LOG_INFO("Handling /userRegister POST request");
			auto bodyString = boost::beast::buffers_to_string(connection->_request.body().data());
			LOG_DEBUG("Received body: {}", bodyString);

			connection->_response.set(boost::beast::http::field::content_type, "text/json");

			json root;
			defer{
				std::string jsonString = root.dump(4);
				boost::beast::ostream(connection->_response.body()) << jsonString;
				return true;
			};

			try {
				LOG_INFO("Parse Json...");
				auto src = json::parse(bodyString);

				std::string email = src["email"].get<std::string>();
				std::string username = src["username"].get<std::string>();
				std::string password = src["password"].get<std::string>();
				std::string verifyCode = src["verify_code"].get<std::string>();


				user::RegisterUserReq req;
				req.set_email(email);
				req.set_username(username);
				req.set_password(password);
				req.set_verify_code(verifyCode);

				auto response = UserGrpcClient::GetInstance()->RegisterUser(req);

				if (response.error() != static_cast<int>(RegisterResponseCodes::REGISTER_SUCCESS)) {
					LOG_WARN("User registration failed for email: {}, error code: {}", email, response.error());
					root["error"] = response.error();
					return;
				}

				root["error"] = response.error();
				root["uid"] = response.uid();
				root["username"] = response.username();
				root["email"] = response.email();
				root["password"] = password;
				root["verify_code"] = verifyCode;
			}
			catch (const json::parse_error& e) {
				LOG_WARN("Failed to parse JSON in /userRegister: {}", e.what());
				root["error"] = static_cast<int>(ErrorCodes::ERROR_JSON);
			}
		}
	);

	RegisterPost(
		"/resetPassword",
		[](std::shared_ptr<HttpConnection> connection) {
			LOG_INFO("Handling /resetPassword POST request");
			auto bodyString = boost::beast::buffers_to_string(connection->_request.body().data());
			LOG_DEBUG("Received body: {}", bodyString);

			connection->_response.set(boost::beast::http::field::content_type, "text/json");

			json root;

			defer{
				std::string jsonString = root.dump(4);
				boost::beast::ostream(connection->_response.body()) << jsonString;
				return true;
			};

			try {
				auto src = json::parse(bodyString);

				std::string uid = src["uid"].get<std::string>();
				std::string email = src["email"].get<std::string>();
				std::string newPassword = src["new_password"].get<std::string>();
				std::string verifyCode = src["verify_code"].get<std::string>();


				user::ResetPasswordReq req;
				req.set_email(email);
				req.set_uid(uid);
				req.set_new_password(newPassword);
				req.set_verify_code(verifyCode);

				auto response = UserGrpcClient::GetInstance()->ResetPassword(req);

				if (response.error() != static_cast<int>(ResetResponseCodes::RESET_SUCCESS)) {
					LOG_WARN("Password reset failed for email: {}, error code: {}", email, response.error());
					root["error"] = response.error();
					return;
				}

				root["error"] = response.error();
				root["uid"] = uid;
				root["email"] = email;
				root["password"] = newPassword;
			}
			catch (const json::parse_error& e) {
				LOG_WARN("Failed to parse JSON in /resetPassword: {}", e.what());
				root["error"] = static_cast<int>(ErrorCodes::ERROR_JSON);
			}
		}
	);

	RegisterPost(
		"/userLogin",
		[](std::shared_ptr<HttpConnection> connection) {
			LOG_INFO("Handling /userLogin POST request");
			auto bodyString = boost::beast::buffers_to_string(connection->_request.body().data());
			LOG_DEBUG("Received body: {}", bodyString);

			connection->_response.set(boost::beast::http::field::content_type, "text/json");

			json root;
			defer{
				std::string jsonString = root.dump(4);
				boost::beast::ostream(connection->_response.body()) << jsonString;
				return true;
			};

			try {
				auto src = json::parse(bodyString);

				std::string uid = src["uid"].get<std::string>();
				std::string email = src["email"].get<std::string>();
				std::string password = src["password"].get<std::string>();

				user::VerifyLoginReq req;
				req.set_uid(uid);
				req.set_email(email);
				req.set_password(password);

				auto response = UserGrpcClient::GetInstance()->VerifyLogin(req);

				if (response.error() != static_cast<int>(LoginResponseCodes::LOGIN_SUCCESS)) {
					LOG_WARN("User login failed for user: {}, error code: {}", email.empty()?uid:email, response.error());
					root["error"] = response.error();
					return;
				}


				auto reply = StatusGrpcClient::GetInstance()->GetChatServer(response.uid());
				if (reply.error()) {
					LOG_ERROR("gRPC get chat server failed for uid: {}", response.uid(), reply.error());
					root["error"] = static_cast<int>(LoginResponseCodes::RPC_GET_FAILED);
					return;
				}

				LOG_INFO("Login successful for user: {}", email.empty() ? uid : email);
				root["error"] = static_cast<int>(LoginResponseCodes::LOGIN_SUCCESS);
				root["uid"] = response.uid();
				root["email"] = response.email();
				root["token"] = reply.token();
				root["host"] = reply.host();
				root["port"] = reply.port();

			}
			catch (const json::parse_error& e) {
				LOG_WARN("Failed to parse JSON in /resetPassword: {}", e.what());
				root["error"] = static_cast<int>(ErrorCodes::ERROR_JSON);
			}
		}
	);
}