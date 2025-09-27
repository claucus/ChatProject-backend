#include "MessageNode.h"
#include "const.h"
#include <boost/asio.hpp>

ReceiveNode::ReceiveNode(size_t  messageId, size_t maxLength):
	BaseNode(maxLength),
	_messageId(messageId)
{

}

size_t ReceiveNode::GetId() const
{
	return _messageId;
}

SendNode::SendNode(const char* message, size_t maxLength, size_t messageId):
	BaseNode(maxLength + HEADER_TOTAL_LENGTH),
	_messageId(messageId)
{
	size_t messageIdHost = boost::asio::detail::socket_ops::host_to_network_short(messageId);
	memcpy(_data, &messageIdHost, HEADER_ID_LENGTH);

	size_t maxLengthHost = boost::asio::detail::socket_ops::host_to_network_short(maxLength);
	memcpy(_data + HEADER_ID_LENGTH, &maxLengthHost, HEADER_DATA_LENGTH);

	memcpy(_data + HEADER_TOTAL_LENGTH, message, maxLength);
}

size_t SendNode::GetId() const
{
	return _messageId;
}
