#ifndef _CU_CHANNEL_H_
#define _CU_CHANNEL_H_

#include <deque>
#include <boost/bind.hpp>
#include "coroutine.h"
#include <fast-event-system/sem.h>

namespace cu {

template <typename T> class channel;

template <typename T, typename Function>
typename channel<T>::link link_template(typename std::enable_if<(!std::is_void<typename std::result_of<Function(T)>::type>::value), Function>::type&& func)
{
	return [&func](typename channel<T>::in& source, typename channel<T>::out& yield)
	{
		for (auto& s : source)
		{
			yield(func(s));
		}
	};
}

template <typename T, typename Function>
typename channel<T>::link link_template(typename std::enable_if<(std::is_void<typename std::result_of<Function(T)>::type>::value), Function>::type&& func)
{
	return [&func](typename channel<T>::in& source, typename channel<T>::out& yield)
	{
		for (auto& s : source)
		{
			func(s);
			yield(s);
		}
	};
}

template <typename T>
auto term_receiver(push_type_ptr<T>& receiver)
{
	return [&](typename channel<T>::in& source)
	{
		for (auto& s : source)
		{
			(*receiver)(s);
		}
	};
}

template <typename T, size_t N = 1>
class channel
{
public:
	using in = cu::pull_type<T>;
	using out = cu::push_type<T>;
	using link = cu::link<T>;

	channel()
	{
		_set_tail();
	}

	template <typename Function>
	channel(Function&& f)
	{
		_set_tail();
		_add(std::forward<Function>(f));
	}

	template <typename Function, typename ... Functions>
	channel(Function&& f, Functions&& ... fs)
	{
		_set_tail();
		_add(std::forward<Function>(f), std::forward<Functions>(fs)...);
	}

	void operator()(const T& data)
	{
		_empty.wait();
		(*_coros.front())(data);
		_full.notify();
	}

	T& operator>>(T& data)
	{
		_full.wait();
		data = _buf;
		_empty.notify();
		return data;
	}

	channel<T>& operator<<(const T& data)
	{
		operator()(data);
		return *this;
	}

protected:
	void _set_tail()
	{
		auto r = cu::make_iterator<T>(
			[this](auto& source) {
				if(source)
				{
					std::cout << "source is ready" << std::endl;
				}
				for (auto& s : source)
				{
					std::cout << "received = " << s << std::endl;
					this->_buf = s;
				}
			}
		);
		_coros.emplace_front( cu::make_iterator<T>( term_receiver<T>(r) ) );
		
		// EACH notify increase buffer, 1 = notify, buffer 1
		_empty.notify();
	}

	template <typename Function>
	void _add(Function&& f)
	{
		_coros.emplace_front(cu::make_iterator<T>(boost::bind(link_template<T, Function>(f), _1, boost::ref(*_coros.front().get()))));
	}

	template <typename Function, typename ... Functions>
	void _add(Function&& f, Functions&& ... fs)
	{
		_add(std::forward<Functions>(fs)...);
		_coros.emplace_front(cu::make_iterator<T>(boost::bind(link_template<T, Function>(f), _1, boost::ref(*_coros.front().get()))));
	}
protected:
	std::deque<push_type_ptr<T> > _coros; // use std::stack
	fes::semaphore _full;
	fes::semaphore _empty;
	T _buf; // use std::array
};

}

#endif
