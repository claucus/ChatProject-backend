#include "CServer.h"
#include "UserManager.h"
#include "Logger.h"

CServer::CServer(boost::asio::io_context& ioc, size_t port):
	_ioc(ioc),
	_port(port),
	_acceptor(ioc,boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(),port))
{
	LOG_INFO("Starting server on port {}", port);
	Start();
}

CServer::~CServer()
{
	LOG_INFO("Shutting down server");
}

void CServer::clearSession(std::string session)
{
	if (_sessions.find(session) != _sessions.end()) {
		UserManager::GetInstance()->removeUserSession(_sessions[session]->GetSessionUid());

		{
			std::lock_guard<std::mutex> lock(_mutex);
			_sessions.erase(session);
		}

		LOG_DEBUG("Session {} removed, remaining sessions: {}", session, _sessions.size());
	}
	else {
		LOG_WARN("Attempted to remove non-existent session: {}", session);
	}
}

void CServer::handlerAccept(std::shared_ptr<CSession> newSession, const boost::system::error_code& error)
{
	if (!error) {
		newSession->Start();
		{
			std::lock_guard<std::mutex> lock(_mutex);
			_sessions.insert(std::make_pair(newSession->GetSessionUid(), newSession));
			LOG_INFO("New session accepted - UUID: {}, Total sessions: {}", 
						newSession->GetSessionUid(), _sessions.size());
		}
	}
	else {
		LOG_ERROR("Session accept error: {}", error.what());
	}

	Start();
}

void CServer::Start()
{
	auto& ioc = IOContextPool::GetInstance()->getIOContext();
	std::shared_ptr<CSession> new_session = std::make_shared<CSession>(ioc, this);
	
	LOG_DEBUG("Setting up new session acceptance on port {}", _port);
	
	_acceptor.async_accept(
		new_session->GetSocket(),
		std::bind(&CServer::handlerAccept, this, new_session, std::placeholders::_1)
	);
}
