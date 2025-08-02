#include <iostream>
#include "StatusServerImpl.h"
#include "ConfigManager.h"
#include <boost/asio.hpp>
#include <memory>
#include <thread>
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

	}
	catch (const spdlog::spdlog_ex& ex) {
		std::cerr << "Server log initialization failed: " << ex.what() << std::endl;
	}
}

int main() {
	try {
		initServerLogger();

		auto& configManager = ConfigManager::GetInstance();

		std::string addr(configManager["StatusServer"]["host"] + ":" + configManager["StatusServer"]["port"]);
		StatusServerImpl service;

		grpc::ServerBuilder builder;
		builder.AddListeningPort(addr, grpc::InsecureServerCredentials());
		builder.RegisterService(&service);

		std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
		spdlog::info("Status Server listening on {}", addr);

		boost::asio::io_context ioc;
		boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);

		signals.async_wait(
			[&server,&ioc](const boost::system::error_code& error, int signal) {
				if (!error) {
					spdlog::info("Shutting Down Server...");
					server->Shutdown();
					ioc.stop();
				}
			}
		);
		
		std::thread(
			[&ioc]() {
				ioc.run();
			}
		).detach();

		server->Wait();
	}
	catch (const std::exception& e) {
		std::cerr << "Exception: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}
	return 0;
}