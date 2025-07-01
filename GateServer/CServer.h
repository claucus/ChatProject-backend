#pragma once
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/http.hpp>
#include <memory>

class CServer:public std::enable_shared_from_this<CServer>
{
public:
	CServer(boost::asio::io_context& ioc, unsigned short& port);
	void Start();

private:
	std::shared_ptr <CServer> Shared();
	
	boost::asio::ip::tcp::acceptor _acceptor;
	boost::asio::io_context& _ioc;
};

