#include "const.h"
#include "HttpConnection.h"
#include "LogicSystem.h"
#include "RedisConPool.h"
#include "VerifyGrpcClient.h"
#include "MySQLManager.h"
#include "StatusGrpcClient.h"
#include <spdlog/spdlog.h>
#include <json/json.h>
#include <json/reader.h>
#include <json/value.h>

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

    RegisterGet(
        "/getTest",
        [](std::shared_ptr<HttpConnection> connection) {
            spdlog::info("[LogicSystem] Handling /getTest GET request");
            boost::beast::ostream(connection->_response.body()) << "receive get_text message" << std::endl;
            size_t i = 0;
            for (auto& node : connection->_getParams) {
                i++;
                boost::beast::ostream(connection->_response.body()) << "param " << i << ", key is " << node.first;
                boost::beast::ostream(connection->_response.body()) << ", value is " << node.second << std::endl;
            }
        }
    );

    RegisterPost(
        "/postVerifyCode",
        [](std::shared_ptr<HttpConnection> connection) {
            spdlog::info("[LogicSystem] Handling /postVerifyCode POST request");
            auto bodyString = boost::beast::buffers_to_string(connection->_request.body().data());
            spdlog::debug("[LogicSystem] Received body: {}", bodyString);

            connection->_response.set(boost::beast::http::field::content_type, "text/json");

            Json::Value root;
            Json::Reader reader;
            Json::Value srcRoot;

            bool parseSuccess = reader.parse(bodyString, srcRoot);

            if (!parseSuccess) {
                spdlog::warn("[LogicSystem] Failed to parse JSON in /postVerifyCode");
                root["error"] = static_cast<int>(ErrorCodes::ERROR_JSON);
                std::string jsonString = root.toStyledString();
                boost::beast::ostream(connection->_response.body()) << jsonString;
                return true;
            }
            auto email = srcRoot["email"].asString();
            spdlog::info("[LogicSystem] Requesting verify code for email: {}", email);
            auto response = VerifyGrpcClient::GetInstance()->GetVerifyCode(email);

            root["verifyCode"] = RedisConPool::GetInstance().get(CODE_PREFIX + email).value();
            root["error"] = response.error();
            root["email"] = srcRoot["email"];
            std::string jsonString = root.toStyledString();
            boost::beast::ostream(connection->_response.body()) << jsonString;
            return true;
        }
    );

    RegisterPost(
        "/userRegister",
        [](std::shared_ptr<HttpConnection> connection) {
            spdlog::info("[LogicSystem] Handling /userRegister POST request");
            auto bodyString = boost::beast::buffers_to_string(connection->_request.body().data());
            spdlog::debug("[LogicSystem] Received body: {}", bodyString);

            connection->_response.set(boost::beast::http::field::content_type, "text/json");

            Json::Value root;
            Json::Reader reader;
            Json::Value srcRoot;

            bool parseSuccess = reader.parse(bodyString, srcRoot);

            if (!parseSuccess) {
                spdlog::warn("[LogicSystem] Failed to parse JSON in /userRegister");
                root["error"] = static_cast<int>(ErrorCodes::ERROR_JSON);
                std::string jsonString = root.toStyledString();
                boost::beast::ostream(connection->_response.body()) << jsonString;
                return true;
            }

            auto email = srcRoot["email"].asString();
            auto username = srcRoot["username"].asString();
            auto password = srcRoot["password"].asString();
            auto verifyCode = srcRoot["verifyCode"].asString();

            auto b_verifyCode = RedisConPool::GetInstance().get(CODE_PREFIX + email).value();

            if (b_verifyCode.empty()) {
                spdlog::warn("[LogicSystem] Verify code timeout for email: {}", email);
                root["error"] = static_cast<int>(RegisterResponseCodes::VERIFY_CODE_TIMEOUT);
                std::string jsonString = root.toStyledString();
                boost::beast::ostream(connection->_response.body()) << jsonString;
                return true;
            }

            if (verifyCode != b_verifyCode) {
                spdlog::warn("[LogicSystem] Verify code mismatch for email: {}", email);
                root["error"] = static_cast<int>(RegisterResponseCodes::VERIFY_CODE_ERROR);
                std::string jsonString = root.toStyledString();
                boost::beast::ostream(connection->_response.body()) << jsonString;
                return true;
            }

            std::string uid = "";
            auto registerUser = UserInfo(username, password, email, uid);
            auto verifyResponse = MySQLManager::GetInstance()->RegisterUser(registerUser);

            if (verifyResponse == false) {
                spdlog::warn("[LogicSystem] User already exists: {}", email);
                root["error"] = static_cast<int>(RegisterResponseCodes::USER_EXISTS);
                std::string jsonString = root.toStyledString();
                boost::beast::ostream(connection->_response.body()) << jsonString;
                return true;
            }

            spdlog::info("[LogicSystem] User registered successfully: {}", email);
            root["error"] = static_cast<int>(RegisterResponseCodes::REGISTER_SUCCESS);
            root["uid"] = registerUser._uid;
            root["username"] = registerUser._name;
            root["email"] = registerUser._email;
            root["password"] = registerUser._password;
            root["verifyCode"] = verifyCode;
            std::string jsonString = root.toStyledString();
            boost::beast::ostream(connection->_response.body()) << jsonString;
            return true;
        }
    );

    RegisterPost(
        "/resetPassword",
        [](std::shared_ptr<HttpConnection> connection) {
            spdlog::info("[LogicSystem] Handling /resetPassword POST request");
            auto bodyString = boost::beast::buffers_to_string(connection->_request.body().data());
            spdlog::debug("[LogicSystem] Received body: {}", bodyString);

            connection->_response.set(boost::beast::http::field::content_type, "text/json");

            Json::Value root;
            Json::Reader reader;
            Json::Value srcRoot;

            bool parseSuccess = reader.parse(bodyString, srcRoot);

            if (!parseSuccess) {
                spdlog::warn("[LogicSystem] Failed to parse JSON in /resetPassword");
                root["error"] = static_cast<int>(ErrorCodes::ERROR_JSON);
                std::string jsonString = root.toStyledString();
                boost::beast::ostream(connection->_response.body()) << jsonString;
                return true;
            }

            auto uid = srcRoot["uuid"].asString();
            auto email = srcRoot["email"].asString();
            auto newPassword = srcRoot["newPassword"].asString();
            auto verifyCode = srcRoot["verifyCode"].asString();

            auto b_verifyCode = RedisConPool::GetInstance().get(CODE_PREFIX + email).value();

            if (b_verifyCode.empty()) {
                spdlog::warn("[LogicSystem] Verify code timeout for email: {}", email);
                root["error"] = static_cast<int>(ResetResponseCodes::VERIFY_CODE_TIMEOUT);
                std::string jsonString = root.toStyledString();
                boost::beast::ostream(connection->_response.body()) << jsonString;
                return true;
            }

            if (verifyCode != b_verifyCode) {
                spdlog::warn("[LogicSystem] Verify code mismatch for email: {}", email);
                root["error"] = static_cast<int>(ResetResponseCodes::VERIFY_CODE_ERROR);
                std::string jsonString = root.toStyledString();
                boost::beast::ostream(connection->_response.body()) << jsonString;
                return true;
            }

            auto resetUser = UserInfo("", newPassword, email, uid);
            auto resetResponse = MySQLManager::GetInstance()->ResetPassword(resetUser);

            if (resetResponse == false) {
                spdlog::warn("[LogicSystem] User not exists for reset: {}", email);
                root["error"] = static_cast<int>(ResetResponseCodes::USER_NOT_EXISTS);
                std::string jsonString = root.toStyledString();
                boost::beast::ostream(connection->_response.body()) << jsonString;
                return true;
            }

            spdlog::info("[LogicSystem] Password reset successful for user: {}", email);
            root["error"] = static_cast<int>(ResetResponseCodes::RESET_SUCCESS);
            root["uid"] = resetUser._uid;
            root["email"] = resetUser._email;
            root["password"] = resetUser._password;

            std::string jsonString = root.toStyledString();
            boost::beast::ostream(connection->_response.body()) << jsonString;
            return true;
        }
    );

    RegisterPost(
        "/userLogin",
        [](std::shared_ptr<HttpConnection> connection) {
            spdlog::info("[LogicSystem] Handling /userLogin POST request");
            auto bodyString = boost::beast::buffers_to_string(connection->_request.body().data());
            spdlog::debug("[LogicSystem] Received body: {}", bodyString);

            connection->_response.set(boost::beast::http::field::content_type, "text/json");

            Json::Value root;
            Json::Reader reader;
            Json::Value srcRoot;

            bool parseSuccess = reader.parse(bodyString, srcRoot);

            if (!parseSuccess) {
                spdlog::warn("[LogicSystem] Failed to parse JSON in /userLogin");
                root["error"] = static_cast<int>(ErrorCodes::ERROR_JSON);
                std::string jsonString = root.toStyledString();
                boost::beast::ostream(connection->_response.body()) << jsonString;
                return true;
            }

            auto uid = srcRoot["uuid"].asString();
            auto email = srcRoot["email"].asString();
            auto password = srcRoot["password"].asString();

            auto loginUser = UserInfo("", password, email, uid);
            auto loginResponse = MySQLManager::GetInstance()->UserLogin(loginUser);

            if (loginResponse == false) {
                spdlog::warn("[LogicSystem] Login failed for user: {}", email);
                root["error"] = static_cast<int>(LoginResponseCodes::LOGIN_ERROR);
                std::string jsonString = root.toStyledString();
                boost::beast::ostream(connection->_response.body()) << jsonString;
                return true;
            }

            auto reply = StatusGrpcClient::GetInstance()->GetChatServer(loginUser._uid);
            if (reply.error()) {
                spdlog::error("[LogicSystem] gRPC get chat server failed for uid: {}", loginUser._uid, reply.error());
                root["error"] = static_cast<int>(LoginResponseCodes::RPC_GET_FAILED);
                std::string jsonString = root.toStyledString();
                boost::beast::ostream(connection->_response.body()) << jsonString;
                return true;
            }

            spdlog::info("[LogicSystem] Login successful for user: {}", email);
            root["error"] = static_cast<int>(LoginResponseCodes::LOGIN_SUCCESS);
            root["uid"] = loginUser._uid;
            root["email"] = loginUser._email;
            root["token"] = reply.token();
            root["host"] = reply.host();
            root["port"] = reply.port();

            std::string jsonString = root.toStyledString();
            boost::beast::ostream(connection->_response.body()) << jsonString;
            return true;
        }
    );
}