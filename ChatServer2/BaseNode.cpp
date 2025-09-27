#include "BaseNode.h"
#include <cstring>
#include "const.h"
#include <boost/asio.hpp>

BaseNode::BaseNode(size_t maxLength):
	_totalLength(maxLength),
	_currentLength(0)
{
	_data = new char[_totalLength + 1];
	_data[_totalLength] = '\0';
}

BaseNode::~BaseNode()
{
	delete[] _data;
	_data = nullptr;
	_totalLength = 0;
	_currentLength = 0;
}

void BaseNode::Clear()
{
	::memset(_data, '\0', _totalLength);
	_currentLength = 0;
}
