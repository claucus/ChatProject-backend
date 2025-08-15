#pragma once

#include <functional>

namespace defer_impl {
	template<typename F>
	class Defer {
	public:
		explicit Defer(F f) :func(std::move(f)), active(true) {

		}
		Defer(Defer&& other) noexcept : func(std::move(other.func), active(other.active)) {
			other.active = false;
		}
		~Defer() {
			if (active) {
				func();
			}
		}

		Defer(const Defer&) = delete;
		Defer& operator=(const Defer&) = delete;
	private:
		F func;
		bool active;
	};

	struct DeferHelper
	{
		template<typename F>
		Defer<F> operator+(F f) {
			return Defer<F>(f);
		}
	};
}


#define CONCATENATE_DETAIL(x,y) x##y
#define CONCATENATE(x,y) CONCATENATE_DETAIL(x,y)
#define defer auto CONCATENATE(_defer_,__COUNTER__) = ::defer_impl::DeferHelper() + [&]()