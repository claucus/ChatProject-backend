#pragma once
class BaseNode
{
public:
	BaseNode(size_t maxLength);
	~BaseNode();
	void Clear();

	char* _data;
	size_t _totalLength;
	size_t _currentLength;
};
