#include <iostream>
#include "CServer.h"
#include "ConfigManager.h"
#include "Logger.h"

int main()
{
	try {
		Logger::init("logs/server.log", (1 << 23), 5);

		auto& gConfigManager = ConfigManager::GetInstance();
		unsigned short portNumber = std::stoi(gConfigManager["GateServer"]["port"]);

		boost::asio::io_context ioc{ 1 };
		boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
		signals.async_wait(
			[&ioc](const boost::system::error_code& error, int singalNumber) {
				if (error) {
					return;
				}
				ioc.stop();
			}
		);

		std::make_shared<CServer>(ioc, portNumber)->Start();
		LOG_INFO("io_context is running on the port: {}", portNumber);
		ioc.run();
	}
	catch (const std::exception& e) {
		LOG_ERROR("Exception: {}", e.what());
		return EXIT_FAILURE;
	}
}
