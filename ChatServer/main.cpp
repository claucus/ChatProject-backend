#include <iostream>
#include "ConfigManager.h"
#include "IOContextPool.h"
#include "CServer.h"
#include "ChatServerImpl.h"
#include "RedisConPool.h"
#include "const.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>

void initServerLogger()
{
	try {
		// 创建多个日志接收器
		std::vector<spdlog::sink_ptr> sinks;

		// 控制台输出
		auto console_sink = std::make_shared<spdlog::sinks::wincolor_stdout_sink_mt>();
		console_sink->set_level(spdlog::level::debug);
		sinks.push_back(console_sink);

		// 文件输出（按大小轮转）
		auto rotating_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
			"logs/server.log",    // 基础文件名
			1024 * 1024 * 5,     // 5MB 文件大小
			3                     // 保留3个历史文件
		);
		rotating_sink->set_level(spdlog::level::trace);
		sinks.push_back(rotating_sink);

		// 创建复合logger
		auto logger = std::make_shared<spdlog::logger>("server_logger", sinks.begin(), sinks.end());

		// 设置日志格式
		logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [thread %t] %v");

		// 设置全局logger
		spdlog::set_default_logger(logger);

		// 设置日志级别（可以通过配置文件配置）
#ifdef _DEBUG
		spdlog::set_level(spdlog::level::debug);
#else
		spdlog::set_level(spdlog::level::info);
#endif

		// 错误级别日志立即刷新
		spdlog::flush_on(spdlog::level::err);
		spdlog::info("Logger initialization completed successfully");
	}
	catch (const spdlog::spdlog_ex& ex) {
		std::cerr << "Server log initialization failed: " << ex.what() << std::endl;
	}
}

int main() {
	try {
		initServerLogger();

		auto& cfg = ConfigManager::GetInstance();
		auto serverName = cfg["SelfServer"]["name"];
		spdlog::info("Starting {} server...", serverName);

		spdlog::debug("Initializing Redis connection pool");
		RedisConPool::GetInstance().hset(ChatServiceConstant::LOGIN_COUNT,serverName,"0");
		spdlog::debug("Redis login count initialized for server: {}", serverName);

		spdlog::debug("Initializing IO Context Pool");
		auto pool = IOContextPool::GetInstance();

		std::string addr(cfg["SelfServer"]["host"] + ":" + cfg["SelfServer"]["RPCPort"]);
		spdlog::debug("Configuring gRPC server on address: {}", addr);


		ChatServerImpl service;
		grpc::ServerBuilder builder;
		builder.AddListeningPort(addr, grpc::InsecureServerCredentials());
		builder.RegisterService(&service);

		std::unique_ptr<grpc::Server> grpcServer(builder.BuildAndStart());
		spdlog::info("gRPC Chat Server listening on {}", addr);


		std::thread grpcServerThread([&grpcServer]() {
			spdlog::debug("Starting gRPC server thread");
			grpcServer->Wait();
		});


		boost::asio::io_context ioc;
		boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
		signals.async_wait([&ioc, pool,&grpcServer](auto, auto) {
			spdlog::info("Shutdown signal received, initiating graceful shutdown");
			ioc.stop();
			pool->Stop();
			grpcServer->Shutdown();
			spdlog::info("Server shutdown completed");
		});

		auto port = cfg["SelfServer"]["port"];
		spdlog::debug("Initializing TCP server on port: {}", port);
		CServer server(ioc,atoi(port.c_str()));

		spdlog::info("Chat Server is running on the port: {}", port);
		ioc.run();

		spdlog::info("Cleaning up server resources");
		RedisConPool::GetInstance().hdel(ChatServiceConstant::LOGIN_COUNT, serverName);
		if (grpcServerThread.joinable()) {
			grpcServerThread.join();
		}
		spdlog::info("Server shutdown completed successfully");
	}
	catch (const std::exception& e) {
		std::cerr << "Exception: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}
}