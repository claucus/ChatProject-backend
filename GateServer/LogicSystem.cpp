#include "const.h"
#include "HttpConnection.h"
#include "LogicSystem.h"
#include "RedisConPool.h"
#include "VerifyGrpcClient.h"
#include "MySQLManager.h"
#include "StatusGrpcClient.h"
#include "Defer.h"
#include <spdlog/spdlog.h>

#include <nlohmann/json.hpp>
using json = nlohmann::json;


bool LogicSystem::HandleGet(std::string path, std::shared_ptr<HttpConnection> con)
{
    if (_getHandlers.find(path) == _getHandlers.end()) {
        spdlog::warn("[LogicSystem] GET handler not found for path: {}", path);
        return false;
    }
    spdlog::info("[LogicSystem] Handling GET request for path: {}", path);
    _getHandlers[path](con);
    return true;
}

bool LogicSystem::HandlePost(std::string path, std::shared_ptr<HttpConnection> con)
{
    if (_postHandlers.find(path) == _postHandlers.end()) {
        spdlog::warn("[LogicSystem] POST handler not found for path: {}", path);
        return false;
    }
    spdlog::info("[LogicSystem] Handling POST request for path: {}", path);
    _postHandlers[path](con);
    return true;
}

void LogicSystem::RegisterGet(std::string url, HttpHandler handler)
{
    spdlog::debug("[LogicSystem] Registering GET handler for url: {}", url);
    _getHandlers.insert(std::make_pair(url, handler));
}

void LogicSystem::RegisterPost(std::string url, HttpHandler handler)
{
    spdlog::debug("[LogicSystem] Registering POST handler for url: {}", url);
    _postHandlers.insert(std::make_pair(url, handler));
}

LogicSystem::LogicSystem()
{
    spdlog::info("[LogicSystem] Initializing LogicSystem and registering HTTP handlers");

    RegisterPost(
        "/postVerifyCode",
        [](std::shared_ptr<HttpConnection> connection) {
            spdlog::info("[LogicSystem] Handling /postVerifyCode POST request");
            auto bodyString = boost::beast::buffers_to_string(connection->_request.body().data());
            spdlog::debug("[LogicSystem] Received body: {}", bodyString);

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
				spdlog::info("[LogicSystem] Requesting verify code for email: {}", email);
				auto response = VerifyGrpcClient::GetInstance()->GetVerifyCode(email);

                root["error"] = response.error();
                root["email"] = email;
				root["verifyCode"] = RedisConPool::GetInstance().get(CODE_PREFIX + email).value();
            }
            catch (const json::parse_error& e) {
				spdlog::warn("[LogicSystem] Failed to parse JSON in /postVerifyCode: {}",e.what());
				root["error"] = static_cast<int>(ErrorCodes::ERROR_JSON);
            }
        }
    );

    RegisterPost(
        "/userRegister",
        [](std::shared_ptr<HttpConnection> connection) {
            spdlog::info("[LogicSystem] Handling /userRegister POST request");
            auto bodyString = boost::beast::buffers_to_string(connection->_request.body().data());
            spdlog::debug("[LogicSystem] Received body: {}", bodyString);

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
                std::string username = src["username"].get<std::string>();
                std::string password = src["password"].get<std::string>();
                std::string verifyCode = src["verifyCode"].get<std::string>();

                auto b_verifyCode = RedisConPool::GetInstance().get(CODE_PREFIX + email).value();

                if (b_verifyCode.empty()) {
					spdlog::warn("[LogicSystem] Verify code timeout for email: {}", email);
					root["error"] = static_cast<int>(RegisterResponseCodes::VERIFY_CODE_TIMEOUT);
                }

                if (verifyCode != b_verifyCode) {
					spdlog::warn("[LogicSystem] Verify code mismatch for email: {}", email);
					root["error"] = static_cast<int>(RegisterResponseCodes::VERIFY_CODE_ERROR);
                }
                
                std::string uid = "";
                auto registerUser = UserInfo(uid, email, username, password);
                auto verifyResponse = MySQLManager::GetInstance()->RegisterUser(registerUser);

                if (!verifyResponse) {
					spdlog::warn("[LogicSystem] User already exists: {}", email);
					root["error"] = static_cast<int>(RegisterResponseCodes::USER_EXISTS);
                }

				spdlog::info("[LogicSystem] User registered successfully: {}", email);
				root["error"] = static_cast<int>(RegisterResponseCodes::REGISTER_SUCCESS);
				root["uid"] = registerUser._uid;
				root["username"] = registerUser._username;
				root["email"] = registerUser._email;
				root["password"] = registerUser._password;
				root["verifyCode"] = verifyCode;

			}
			catch (const json::parse_error& e) {
				spdlog::warn("[LogicSystem] Failed to parse JSON in /userRegister: {}", e.what());
				root["error"] = static_cast<int>(ErrorCodes::ERROR_JSON);
			}
        }
    );

	RegisterPost(
		"/resetPassword",
		[](std::shared_ptr<HttpConnection> connection) {
			spdlog::info("[LogicSystem] Handling /resetPassword POST request");
			auto bodyString = boost::beast::buffers_to_string(connection->_request.body().data());
			spdlog::debug("[LogicSystem] Received body: {}", bodyString);

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

                auto b_verifyCode = RedisConPool::GetInstance().get(CODE_PREFIX + email).value();

                if (b_verifyCode.empty()) {
					spdlog::warn("[LogicSystem] Verify code timeout for email: {}", email);
					root["error"] = static_cast<int>(ResetResponseCodes::VERIFY_CODE_TIMEOUT);
                }

                if (verifyCode != b_verifyCode) {
					spdlog::warn("[LogicSystem] Verify code mismatch for email: {}", email);
					root["error"] = static_cast<int>(ResetResponseCodes::VERIFY_CODE_ERROR);
                }

                auto resetUser = UserInfo(uid,email,"",newPassword);
                auto resetResponse = MySQLManager::GetInstance()->ResetPassword(resetUser);

                if (!resetResponse) {
					spdlog::warn("[LogicSystem] User not exists for reset: {}", email);
					root["error"] = static_cast<int>(ResetResponseCodes::USER_NOT_EXISTS);
                }
                spdlog::info("[LogicSystem] Password reset successful for user: {}", email);
				root["error"] = static_cast<int>(ResetResponseCodes::RESET_SUCCESS);
				root["uid"] = resetUser._uid;
				root["email"] = resetUser._email;
				root["password"] = resetUser._password;
            }
            catch (const json::parse_error& e) {
				spdlog::warn("[LogicSystem] Failed to parse JSON in /resetPassword: {}",e.what());
				root["error"] = static_cast<int>(ErrorCodes::ERROR_JSON);
            }
		}
	);

	RegisterPost(
		"/userLogin",
		[](std::shared_ptr<HttpConnection> connection) {
			spdlog::info("[LogicSystem] Handling /userLogin POST request");
			auto bodyString = boost::beast::buffers_to_string(connection->_request.body().data());
			spdlog::debug("[LogicSystem] Received body: {}", bodyString);

			connection->_response.set(boost::beast::http::field::content_type, "text/json");

            json root;
            defer{
                std::string jsonString = root.dump(4);
			    boost::beast::ostream(connection->_response.body()) << jsonString;
			    return true;
            };
            try {
                auto src = json::parse(bodyString);

                std::string uid = root["uid"].get<std::string>();
                std::string email = root["email"].get<std::string>();
                std::string password = root["password"].get<std::string>();

                auto loginUser = UserInfo(uid, email, "", password);
				auto loginResponse = MySQLManager::GetInstance()->UserLogin(loginUser);

				if (loginResponse == false) {
					spdlog::warn("[LogicSystem] Login failed for user: {}", email);
					root["error"] = static_cast<int>(LoginResponseCodes::LOGIN_ERROR);
				}

				auto reply = StatusGrpcClient::GetInstance()->GetChatServer(loginUser._uid);
				if (reply.error()) {
					spdlog::error("[LogicSystem] gRPC get chat server failed for uid: {}", loginUser._uid, reply.error());
					root["error"] = static_cast<int>(LoginResponseCodes::RPC_GET_FAILED);
				}

				spdlog::info("[LogicSystem] Login successful for user: {}", email);
				root["error"] = static_cast<int>(LoginResponseCodes::LOGIN_SUCCESS);
				root["uid"] = loginUser._uid;
				root["email"] = loginUser._email;
				root["token"] = reply.token();
				root["host"] = reply.host();
				root["port"] = reply.port();

            }
            catch (const json::parse_error& e) {
				spdlog::warn("[LogicSystem] Failed to parse JSON in /resetPassword: {}",e.what());
				root["error"] = static_cast<int>(ErrorCodes::ERROR_JSON);
            }
		}
	);
}