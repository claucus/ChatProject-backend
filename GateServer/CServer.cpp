#include "CServer.h"
#include "HttpConnection.h"
#include "IOContextPool.h"
#include <spdlog/spdlog.h>

CServer::CServer(boost::asio::io_context& ioc, unsigned short& portNumber) :
    _ioc(ioc),
    _acceptor(ioc, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), portNumber))
{
    spdlog::info("[CServer] Server initialized on port {}", portNumber);
}

void CServer::Start()
{
    auto self = Shared();

    auto& io_context = IOContextPool::GetInstance()->getIOContext();
    std::shared_ptr<HttpConnection> newCon = std::make_shared<HttpConnection>(io_context);

    _acceptor.async_accept(
        newCon->GetSocket(),
        [self, newCon](boost::beast::error_code ec) {
            try {
                if (ec) {
                    spdlog::error("[CServer] Accept error: {}", ec.message());
                    self->Start();
                    return;
                }

                spdlog::info("[CServer] New HTTP connection accepted");
                newCon->Start();

                self->Start();
            }
            catch (const std::exception& e) {
                spdlog::critical("[CServer] Exception in accept handler: {}", e.what());
                self->Start();
            }
        }
    );
}

std::shared_ptr<CServer> CServer::Shared()
{
    return shared_from_this();
}
