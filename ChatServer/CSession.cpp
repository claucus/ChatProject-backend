#include "CSession.h"
#include "CServer.h"
#include "LogicNode.h"
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/random_generator.hpp>
#include "LogicSystem.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>

CSession::CSession(boost::asio::io_context& ioc, CServer* server) :
	_socket(ioc),
	_server(server),
	_b_close(false),
	_b_head_parse(true)
{
	auto a_uuid = boost::uuids::random_generator()();
	_uuid = boost::uuids::to_string(a_uuid);
	spdlog::info("[Session {}] Created new session", _uuid);

	_receiveHeadNode = std::make_shared<ReceiveNode>(HEADER_TOTAL_LENGTH, 0);
	_receiveMessageNode = nullptr;
}

CSession::~CSession()
{
	spdlog::info("[Session {}] Destroying session", _uuid);
	Close();
}

boost::asio::ip::tcp::socket& CSession::GetSocket()
{
	return _socket;
}

std::string& CSession::GetUuid()
{
	return _uuid;
}

void CSession::Start()
{
	spdlog::info("[Session {}] Starting session", _uuid);
	auto self = Shared();
	_socket.async_read_some(
		boost::asio::buffer(_buffer, BUFFER_SIZE),
		std::bind(&CSession::handleRead, this, std::placeholders::_1, std::placeholders::_2, Shared())
	);
}

void CSession::Close()
{
	if (!_b_close) {
		spdlog::info("[Session {}] Closing session", _uuid);
		_b_close = true;
		boost::system::error_code ec;
		_socket.close(ec);
		if (_server) {
			_server->clearSession(_uuid);
		}
	}
}

void CSession::Send(char* message, size_t maxLength, size_t messageId)
{
	auto node = std::make_shared<SendNode>(message, maxLength, messageId);
	{
		std::lock_guard<std::mutex> lock(_sendMutex);
		_sendQueue.push(node);
	}
	
	spdlog::debug("[Session {}] Queued message for sending, ID: {}, Length: {}", 
				  _uuid, messageId, maxLength);
	
	auto self = Shared();
	handleWrite(boost::system::error_code(), self);
}

void CSession::Send(std::string message, size_t messageId)
{
	Send((char*)message.c_str(), message.length(), messageId);
}

std::shared_ptr<CSession> CSession::Shared()
{
	return shared_from_this();
}

void CSession::handleRead(const boost::system::error_code& error, size_t bytesTransferred, std::shared_ptr<CSession> self)
{
	if (error) {
		spdlog::error("[Session {}] Read error: {}", _uuid, error.message());
		Close();
		return;
	}

	spdlog::trace("[Session {}] Received {} bytes", _uuid, bytesTransferred);

	size_t proceessBytes = 0;
	while (proceessBytes < bytesTransferred) {
		if (_b_head_parse) {
			size_t remainHeaderBytes = HEADER_TOTAL_LENGTH - _receiveHeadNode->_currentLength;
			size_t availableBytes = bytesTransferred - proceessBytes;
			size_t bytesToCopy = std::min(remainHeaderBytes, availableBytes);

			memcpy(
				_receiveHeadNode->_data + _receiveHeadNode->_currentLength,
				_buffer + proceessBytes,
				bytesToCopy
			);
			_receiveHeadNode->_currentLength += bytesToCopy;
			proceessBytes += bytesToCopy;

			if (_receiveHeadNode->_currentLength == HEADER_TOTAL_LENGTH) {
				uint16_t messageId = *reinterpret_cast<uint16_t*>(_receiveHeadNode->_data);
				uint16_t messageLength = *reinterpret_cast<uint16_t*>(_receiveHeadNode->_data + HEADER_ID_LENGTH);

				messageId = boost::asio::detail::socket_ops::network_to_host_short(messageId);
				messageLength = boost::asio::detail::socket_ops::network_to_host_short(messageLength);

				spdlog::debug("[Session {}] Header parsed - Message ID: {}, Length: {}", 
							_uuid, messageId, messageLength);

				if (messageLength > MAX_LENGTH) {
					spdlog::error("[Session {}] Message length {} exceeds maximum allowed {}", 
								_uuid, messageLength, MAX_LENGTH);
					Close();
					return;
				}

				_receiveMessageNode = std::make_shared<ReceiveNode>(messageId, messageLength);
				_b_head_parse = false;
			}
		}
		else {
			size_t remainMessageBytes = _receiveMessageNode->_totalLength - _receiveMessageNode->_currentLength;
			size_t availableBytes = bytesTransferred - proceessBytes;
			size_t bytesToCopy = std::min(remainMessageBytes, availableBytes);

			memcpy(
				_receiveMessageNode->_data + _receiveMessageNode->_currentLength,
				_buffer + proceessBytes,
				bytesToCopy
			);
			_receiveMessageNode->_currentLength += bytesToCopy;
			proceessBytes += bytesToCopy;

			if (_receiveMessageNode->_currentLength == _receiveMessageNode->_totalLength) {
				spdlog::debug("[Session {}] Message fully received, posting to LogicSystem", _uuid);
				
				auto logicNode = std::make_shared<LogicNode>(self, _receiveMessageNode);
				LogicSystem::GetInstance()->PostMessageToQueue(logicNode);

				_receiveHeadNode->Clear();
				_b_head_parse = true;
			}
		}
	}

	_socket.async_read_some(
		boost::asio::buffer(_buffer, BUFFER_SIZE),
		std::bind(&CSession::handleRead, this, std::placeholders::_1, std::placeholders::_2, Shared())
	);
}

void CSession::handleWrite(const boost::system::error_code& error, std::shared_ptr<CSession> self)
{
	if (error) {
		spdlog::error("[Session {}] Write error: {}", _uuid, error.message());
		Close();
		return;
	}

	std::shared_ptr<SendNode> node;
	{
		std::lock_guard<std::mutex> lock(_sendMutex);
		if (_sendQueue.empty()) {
			return;
		}

		node = _sendQueue.front();
		_sendQueue.pop();
	}

	spdlog::debug("[Session {}] Sending message, ID: {}, Length: {}", 
				  _uuid, node->GetId(), node->_totalLength);

	boost::asio::async_write(
		_socket,
		boost::asio::buffer(node->_data, node->_totalLength),
		std::bind(&CSession::handleWrite,this,std::placeholders::_1,Shared())
	);
}
