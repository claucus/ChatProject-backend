#pragma once
#include <mutex>
#include <memory>

template<typename T>
class Singleton {
public:
	static std::shared_ptr<T> GetInstance() {
		static std::once_flag _flag;
		std::call_once(
			_flag, [&]() {
				_instance = std::shared_ptr<T>(new T);
			}
		);
		return _instance;
	}
	~Singleton() = default;

protected:
	static std::shared_ptr<T> _instance;

	Singleton() = default;
	Singleton(const Singleton<T>&) = delete;
	Singleton& operator = (const Singleton<T>&) = delete;
};

template<typename T>
std::shared_ptr<T> Singleton<T>::_instance = nullptr;