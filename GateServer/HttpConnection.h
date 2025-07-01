#pragma once
#include <boost/asio.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast.hpp>
#include <unordered_map>
#include <memory>
#include <chrono>


class HttpConnection :public std::enable_shared_from_this<HttpConnection>
{
	friend class LogicSystem;
public:
	HttpConnection(boost::asio::io_context& ioc);
	void Start();
	boost::asio::ip::tcp::socket& GetSocket();

private:
	std::shared_ptr<HttpConnection> Shared();

	void CheckDeadline();
	void WriteResponse();
	void HandleRequest();

	void PreParseGetParam();

	boost::asio::ip::tcp::socket _socket;
	boost::beast::flat_buffer _buffer{ 8192 };
	boost::beast::http::request<boost::beast::http::dynamic_body> _request;
	boost::beast::http::response<boost::beast::http::dynamic_body> _response;
	boost::asio::steady_timer _deadline{
		_socket.get_executor(),std::chrono::seconds(60)
	};

	std::string _getUrl;
	std::unordered_map<std::string, std::string>  _getParams;
};

