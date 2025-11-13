#pragma once
#include <boost/asio.hpp>
#include <map>
#include <memory>
#include <mutex>
#include "CSession.h"
#include "IOContextPool.h"

class CServer
{
public:
	CServer(boost::asio::io_context& ioc, size_t port);
	~CServer();
	void clearSession(std::string session);

private:
	boost::asio::io_context& _ioc;
	boost::asio::ip::tcp::acceptor _acceptor;
	std::mutex _mutex;
	size_t _port;

	std::map<std::string, std::shared_ptr<CSession>> _sessions;

private:
	void handlerAccept(std::shared_ptr<CSession>,const boost::system::error_code & error);
	void Start();
};

