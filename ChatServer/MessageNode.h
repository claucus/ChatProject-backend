#pragma once
#include "BaseNode.h"

class ReceiveNode :public BaseNode {
public:
	ReceiveNode(size_t maxLength, size_t messageId);
	size_t GetId() const;

private:
	size_t _messageId;
};

class SendNode :public BaseNode {
public:
	SendNode(const char* message, size_t maxLength, size_t messageId);
	size_t GetId() const;

private:
	size_t _messageId;
};