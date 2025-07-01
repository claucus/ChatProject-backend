#pragma once
#include <memory>
#include "CSession.h"

class LogicNode
{
	friend class LogicSystem;
public:
	LogicNode(std::shared_ptr<CSession> session, std::shared_ptr<ReceiveNode> receiveNode);
private:
	std::shared_ptr<CSession> _session;
	std::shared_ptr<ReceiveNode> _receiveNode;
};
