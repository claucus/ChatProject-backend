#include "HttpConnection.h"
#include "LogicSystem.h"
#include "Logger.h"
#include "Transformer.h"

HttpConnection::HttpConnection(boost::asio::io_context& ioc) :_socket(ioc)
{
	LOG_DEBUG("New connection object created");
}

void HttpConnection::Start()
{
	auto self = Shared();
	LOG_INFO("Start reading HTTP request");
	boost::beast::http::async_read(
		_socket,
		_buffer,
		_request,
		[self](boost::beast::error_code ec, std::size_t bytesTransferred) {
			try {
				if (ec) {
					LOG_ERROR("HTTP read error: {} ({})", ec.message(), ec.value());
					return;
				}
				LOG_DEBUG("HTTP request read, {} bytes transferred", bytesTransferred);
				boost::ignore_unused(bytesTransferred);
				self->HandleRequest();
				self->CheckDeadline();
			}
			catch (const std::exception& e) {
				LOG_CRITICAL("Exception in async_read handler: {}", e.what());
			}
		}
	);
}

boost::asio::ip::tcp::socket& HttpConnection::GetSocket()
{
	return this->_socket;
}

std::shared_ptr<HttpConnection> HttpConnection::Shared()
{
	return shared_from_this();
}

void HttpConnection::CheckDeadline()
{
	auto self = Shared();
	_deadline.async_wait(
		[self](boost::beast::error_code ec) {
			if (!ec) {
				LOG_WARN("Deadline reached, closing socket");
				self->_socket.close(ec);
			}
		}
	);
}

void HttpConnection::WriteResponse()
{
	auto self = Shared();
	_response.content_length(_response.body().size());
	LOG_INFO("Writing HTTP response, status: {}", _response.result_int());
	boost::beast::http::async_write(
		_socket,
		_response,
		[self](boost::beast::error_code ec, std::size_t bytesTransferred) {
			if (ec) {
				LOG_ERROR("HTTP write error: {} ({})", ec.message(), ec.value());
			}
			else {
				LOG_DEBUG("HTTP response sent, {} bytes", bytesTransferred);
			}
			self->_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
			self->_deadline.cancel();
		}
	);
}

void HttpConnection::HandleRequest()
{
	LOG_INFO("Handling HTTP request: {} {}", _request.method_string(), _request.target());
	_response.version(_request.version());
	_response.keep_alive(false);

	if (_request.method() == boost::beast::http::verb::get) {
		PreParseGetParam();
		bool success = LogicSystem::GetInstance()->HandleGet(_getUrl, Shared());
		if (!success) {
			LOG_WARN("GET url not found: {}", _getUrl);
			_response.result(boost::beast::http::status::not_found);
			_response.set(boost::beast::http::field::content_type, "text/plain");
			boost::beast::ostream(_response.body()) << "url not found\r\n";
			WriteResponse();
			return;
		}
		LOG_DEBUG("GET url handled successfully: {}", _getUrl);
		_response.result(boost::beast::http::status::ok);
		_response.set(boost::beast::http::field::server, "GateServer");
		WriteResponse();
		return;
	}

	if (_request.method() == boost::beast::http::verb::post) {
		bool success = LogicSystem::GetInstance()->HandlePost(_request.target(), Shared());
		if (!success) {
			LOG_WARN("POST url not found: {}", _request.target());
			_response.result(boost::beast::http::status::not_found);
			_response.set(boost::beast::http::field::content_type, "text/plain");
			boost::beast::ostream(_response.body()) << "url not found\r\n";
			WriteResponse();
			return;
		}
		LOG_DEBUG("POST url handled successfully: {}", _request.target());
		_response.result(boost::beast::http::status::ok);
		_response.set(boost::beast::http::field::server, "GateServer");
		WriteResponse();
		return;
	}
}

void HttpConnection::PreParseGetParam()
{
	auto url = _request.target();
	LOG_DEBUG("Pre-parsing GET parameters for url: {}", url);

	auto queryIndex = url.find('?');
	if (queryIndex == std::string::npos) {
		_getUrl = url;
		return;
	}

	_getUrl = url.substr(0, queryIndex);
	std::string queryString = url.substr(queryIndex + 1);
	std::string key, value;
	size_t pos = 0;
	while ((pos = queryString.find('&')) != std::string::npos) {
		auto subString = queryString.substr(0, pos);
		size_t equalIndex = subString.find('=');

		if (equalIndex != std::string::npos) {
			key = urlDecoder(subString.substr(0, equalIndex));
			value = urlDecoder(subString.substr(equalIndex + 1));

			_getParams[key] = value;
			LOG_DEBUG("GET param: {}={}", key, value);
		}

		queryString.erase(0, pos + 1);
	}

	if (!queryString.empty()) {
		auto equalIndex = queryString.find('=');
		if (equalIndex != std::string::npos) {
			key = urlDecoder(queryString.substr(0, equalIndex));
			value = urlDecoder(queryString.substr(equalIndex + 1));

			_getParams[key] = value;
			LOG_DEBUG("GET param: {}={}", key, value);
		}
	}
}
