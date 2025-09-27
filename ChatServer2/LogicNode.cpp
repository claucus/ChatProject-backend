#include "LogicNode.h"

LogicNode::LogicNode(std::shared_ptr<CSession> session, std::shared_ptr<ReceiveNode> receiveNode):
	_session(session),_receiveNode(receiveNode)
{
	
}
