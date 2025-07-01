#pragma once
#include "Singleton.h"
#include <memory>
#include <functional>
#include <map>
#include <string>

class HttpConnection;
using HttpHandler = std::function<void(std::shared_ptr<HttpConnection>)>;

class LogicSystem :public Singleton<LogicSystem>
{
	friend class Singleton<LogicSystem>;
public:
	~LogicSystem() = default;
	bool HandleGet(std::string, std::shared_ptr<HttpConnection>);
	bool HandlePost(std::string, std::shared_ptr<HttpConnection>);
	void RegisterGet(std::string, HttpHandler handler);
	void RegisterPost(std::string, HttpHandler handler);
private:
	LogicSystem();
	std::map<std::string, HttpHandler> _postHandlers;
	std::map<std::string, HttpHandler> _getHandlers;
};

