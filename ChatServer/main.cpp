#include <iostream>
#include "ConfigManager.h"
#include "IOContextPool.h"
#include "CServer.h"
#include "FriendServerImpl.h"
#include "RedisConPool.h"
#include "const.h"
#include "Logger.h"

int main() {
	try {
		Logger::init("logs/server.log", (1 << 23), 5);

		auto& cfg = ConfigManager::GetInstance();
		auto serverName = cfg["SelfServer"]["name"];
		LOG_INFO("Starting {} server...", serverName);

		LOG_DEBUG("Initializing Redis connection pool");
		RedisConPool::GetInstance().hset(ChatServiceConstant::LOGIN_COUNT,serverName,"0");
		LOG_DEBUG("Redis login count initialized for server: {}", serverName);

		LOG_DEBUG("Initializing IO Context Pool");
		auto pool = IOContextPool::GetInstance();

		std::string addr(cfg["SelfServer"]["host"] + ":" + cfg["SelfServer"]["RPCPort"]);
		LOG_DEBUG("Configuring gRPC server on address: {}", addr);


		FriendServerImpl service;
		grpc::ServerBuilder builder;
		builder.AddListeningPort(addr, grpc::InsecureServerCredentials());
		builder.RegisterService(&service);

		std::unique_ptr<grpc::Server> grpcServer(builder.BuildAndStart());
		LOG_INFO("gRPC Chat Server listening on {}", addr);


		std::thread grpcServerThread([&grpcServer]() {
			LOG_DEBUG("Starting gRPC server thread");
			grpcServer->Wait();
		});


		boost::asio::io_context ioc;
		boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
		signals.async_wait([&ioc, pool,&grpcServer](auto, auto) {
			LOG_INFO("Shutdown signal received, initiating graceful shutdown");
			ioc.stop();
			pool->Stop();
			grpcServer->Shutdown();
			LOG_INFO("Server shutdown completed");
		});

		auto port = cfg["SelfServer"]["port"];
		LOG_DEBUG("Initializing TCP server on port: {}", port);
		CServer server(ioc,atoi(port.c_str()));

		LOG_INFO("Chat Server is running on the port: {}", port);
		ioc.run();

		LOG_INFO("Cleaning up server resources");
		RedisConPool::GetInstance().hdel(ChatServiceConstant::LOGIN_COUNT, serverName);
		if (grpcServerThread.joinable()) {
			grpcServerThread.join();
		}
		LOG_INFO("Server shutdown completed successfully");
	}
	catch (const std::exception& e) {
		LOG_ERROR("Exception: {}", e.what());
		return EXIT_FAILURE;
	}
}