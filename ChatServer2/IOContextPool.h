#pragma once
#include "Singleton.h"
#include <vector>
#include <boost/asio.hpp>

class IOContextPool :public Singleton<IOContextPool>
{
	friend class Singleton<IOContextPool>;
	using Work = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;
public:
	~IOContextPool();
	IOContextPool(const IOContextPool&) = delete;
	IOContextPool& operator=(const IOContextPool&) = delete;

	boost::asio::io_context& getIOContext();
	void Stop();

private:
	IOContextPool(std::size_t size = std::thread::hardware_concurrency());
	std::vector<boost::asio::io_context> _ioContext;
	std::vector<std::unique_ptr<Work>> _works;
	std::vector < std::thread > _threads;
	std::size_t _nextIOContext;
};

