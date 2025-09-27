#include "IOContextPool.h"

IOContextPool::~IOContextPool()
{
	Stop();
}

boost::asio::io_context& IOContextPool::getIOContext()
{
	auto& context = _ioContext[_nextIOContext++];
	if (_nextIOContext == _ioContext.size()) {
		_nextIOContext = 0;
	}
	return context;
}

void IOContextPool::Stop()
{
	for (auto& work : _works) {
		work->reset();
	}

	for (auto& context : _ioContext) {
		context.stop();
	}

	for (auto& t : _threads) {
		if (t.joinable()) {
			t.join();
		}
	}
}

IOContextPool::IOContextPool(std::size_t size /*= std::thread::hardware_concurrency()*/) :
	_ioContext(size), _works(size), _nextIOContext(0)
{
	for (std::size_t i = 0; i < size; ++i) {
		_works[i] = std::unique_ptr<Work>(new Work(_ioContext[i].get_executor()));
	}

	for (std::size_t i = 0; i < size; ++i) {
		_threads.emplace_back(
			[this, i]() {
				_ioContext[i].run();
			}
		);
	}
}


