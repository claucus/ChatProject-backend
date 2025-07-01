#include "CServer.h"
#include <spdlog/spdlog.h>

CServer::CServer(boost::asio::io_context& ioc, short port):
	_ioc(ioc),
	_port(port),
	_acceptor(ioc,boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(),port))
{
	spdlog::info("[Server] Starting server on port {}", port);
	Start();
}

CServer::~CServer()
{
	spdlog::info("[Server] Shutting down server");
}

void CServer::clearSession(std::string session)
{
	if (_sessions.find(session) != _sessions.end()) {
		std::lock_guard<std::mutex> lock(_mutex);
		_sessions.erase(session);
		spdlog::debug("[Server] Session {} removed, remaining sessions: {}", session, _sessions.size());
	}
	else {
		spdlog::warn("[Server] Attempted to remove non-existent session: {}", session);
	}
}

void CServer::handlerAccept(std::shared_ptr<CSession> newSession, const boost::system::error_code& error)
{
	if (!error) {
		newSession->Start();
		{
			std::lock_guard<std::mutex> lock(_mutex);
			_sessions.insert(std::make_pair(newSession->GetUuid(), newSession));
			spdlog::info("[Server] New session accepted - UUID: {}, Total sessions: {}", 
						newSession->GetUuid(), _sessions.size());
		}
	}
	else {
		spdlog::error("[Server] Session accept error: {}", error.what());
	}

	Start();
}

void CServer::Start()
{
	auto& ioc = IOContextPool::GetInstance()->getIOContext();
	std::shared_ptr<CSession> new_session = std::make_shared<CSession>(ioc, this);
	
	spdlog::debug("[Server] Setting up new session acceptance on port {}", _port);
	
	_acceptor.async_accept(
		new_session->GetSocket(),
		std::bind(&CServer::handlerAccept, this, new_session, std::placeholders::_1)
	);
}
