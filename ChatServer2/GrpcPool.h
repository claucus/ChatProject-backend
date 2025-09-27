#pragma once

#include <string>
#include <memory>
#include <atomic>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <grpcpp/grpcpp.h>
#include "message.grpc.pb.h"

template <typename ServiceType,typename StubType>
class GrpcPool
{
public:
	GrpcPool(size_t poolSize, const std::string& host, const std::string& port):
		_poolSize(poolSize), _host(host), _port(port), _b_stop(false)
	{
		for (size_t i = 0; i < _poolSize; ++i) {
			std::shared_ptr<grpc::Channel> channel = grpc::CreateChannel(
				host + ":" + port,
				grpc::InsecureChannelCredentials()
			);
			_connections.emplace(ServiceType::NewStub(channel));
		}
	}

	~GrpcPool() {
		std::lock_guard<std::mutex> lock(_mutex);
		Close();
		while (!_connections.empty()) {
			_connections.pop();
		}
	}

	std::unique_ptr<StubType> GetConnection() {

		std::unique_lock<std::mutex> lock(_mutex);
		_condition.wait(
			lock,
			[this]() {
				if (_b_stop) {
					return true;
				}
				return !_connections.empty();
			}
		);

		if (_b_stop) {
			return nullptr;
		}

		auto context = std::move(_connections.front());
		_connections.pop();

		return context;
	}

	void ReturnConnection(std::unique_ptr<StubType> context) {
		std::lock_guard<std::mutex> lock(_mutex);
		if (_b_stop) {
			return;
		}
		_connections.emplace(std::move(context));
		_condition.notify_one();
	}
	void Close() {
		_b_stop = true;
		_condition.notify_all();
	}

private:
	std::atomic<bool> _b_stop;
	std::size_t _poolSize;
	std::string _host;
	std::string _port;
	std::queue<std::unique_ptr<StubType> > _connections;
	std::mutex _mutex;
	std::condition_variable _condition;
};