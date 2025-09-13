#include <iostream>
#include <boost/asio.hpp>
#include "StatusServerImpl.h"
#include "ConfigManager.h"
#include "Logger.h"


int main() {
	try {
		Logger::init("logs/server.log", (1 << 23), 5);

		auto& configManager = ConfigManager::GetInstance();

		std::string addr(configManager["StatusServer"]["host"] + ":" + configManager["StatusServer"]["port"]);
		StatusServerImpl service;

		grpc::ServerBuilder builder;
		builder.AddListeningPort(addr, grpc::InsecureServerCredentials());
		builder.RegisterService(&service);

		std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
		LOG_INFO("Status Server listening on {}", addr);

		boost::asio::io_context ioc;
		boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);

		signals.async_wait(
			[&server,&ioc](const boost::system::error_code& error, int signal) {
				if (!error) {
					LOG_INFO("Shutting Down Server...");
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
		LOG_ERROR("Exception: {}", e.what());
		return EXIT_FAILURE;
	}
	return 0;
}