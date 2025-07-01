#include "VerifyGrpcClient.h"
#include "ConfigManager.h"
#include <spdlog/spdlog.h>

message::GetVerifyResponse VerifyGrpcClient::GetVerifyCode(std::string email)
{
    grpc::ClientContext context;
    message::GetVerifyRequest request;
    message::GetVerifyResponse response;

    request.set_email(email);
    auto stub = _pool->GetConnection();
    spdlog::info("[VerifyGrpcClient] Calling GetVerifyCode for email={}", email);
    auto status = stub->GetVerifyCode(&context, request, &response);

    if (!status.ok()) {
        spdlog::error("[VerifyGrpcClient] gRPC GetVerifyCode failed: {} ", status.error_message());
        response.set_error(static_cast<int>(ErrorCodes::RPC_FAILED));
    } else {
        spdlog::debug("[VerifyGrpcClient] gRPC GetVerifyCode succeeded for email={}", email);
    }
    _pool->ReturnConnection(std::move(stub));
    return response;
}

VerifyGrpcClient::VerifyGrpcClient()
{
    auto& configManager = ConfigManager::GetInstance();
    std::string host = configManager["VerifyServer"]["host"];
    std::string port = configManager["VerifyServer"]["port"];

    spdlog::info("[VerifyGrpcClient] Initializing with host: {}, port: {}", host, port);

    _pool.reset(new VerifyConPool((size_t)(std::thread::hardware_concurrency()), host, port));
}
