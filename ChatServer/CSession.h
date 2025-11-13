#pragma once
#include <boost/asio.hpp>
#include <memory>
#include <atomic>
#include <mutex>
#include <queue>
#include "const.h"
#include "MessageNode.h"

class CServer;
class LogicSystem;

class CSession:public std::enable_shared_from_this<CSession>
{
public:
	CSession(boost::asio::io_context & ioc,CServer * server);
	~CSession();

	boost::asio::ip::tcp::socket& GetSocket();
	std::string& GetSessionUid();

	void SetUserUid(const std::string& uid);
	std::string GetUserUid() const;

	void Start();
	void Close();
	
	void Send(char* message, size_t maxLength, size_t messageId);
	void Send(std::string message, size_t messageId);

private:
	char _buffer[BUFFER_SIZE];

	boost::asio::ip::tcp::socket _socket;
	std::string _sessionUid;
	std::string _userUid;

	CServer* _server;
	std::atomic<bool> _b_close;

	std::queue<std::shared_ptr<SendNode>> _sendQueue;
	std::mutex _sendMutex;

	std::atomic<bool> _b_head_parse;
	std::shared_ptr<ReceiveNode> _receiveMessageNode;
	std::shared_ptr<ReceiveNode> _receiveHeadNode;

private:
	std::shared_ptr<CSession> Shared();
	void handleRead(const boost::system::error_code & error,size_t bytesTransferred,std::shared_ptr<CSession> self);
	void handleWrite(const boost::system::error_code& error, std::shared_ptr<CSession> self);
};

