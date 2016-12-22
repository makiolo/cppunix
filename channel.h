#ifndef _CU_CHANNEL_H_
#define _CU_CHANNEL_H_

#include <deque>
#include <stack>
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
auto term_receiver(const push_type_ptr<T>& receiver)
{
	return [=](typename channel<T>::in& source)
	{
		for (auto& s : source)
		{
			(*receiver)(s);
		}
	};
}

template <typename T>
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
		(*_coros.top())(data);
		_full.notify();
	}

	T& operator>>(T& data)
	{
		_full.wait();
		data = _buf.top();
		_buf.pop();
		_empty.notify();
		return data;
	}
	
	T get()
	{
		T data;
		operator>>(data);
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
				for (auto& s : source)
				{
					this->_buf.push(s);
				}
			}
		);
		_coros.push( cu::make_iterator<T>( term_receiver<T>(r) ) );
		
		// EACH notify increase buffer, 1 = notify, buffer 1
		_empty.notify();
	}

	template <typename Function>
	void _add(Function&& f)
	{
		_coros.push(cu::make_iterator<T>(boost::bind(link_template<T, Function>(f), _1, boost::ref(*_coros.front().get()))));
	}

	template <typename Function, typename ... Functions>
	void _add(Function&& f, Functions&& ... fs)
	{
		_add(std::forward<Functions>(fs)...);
		_coros.push(cu::make_iterator<T>(boost::bind(link_template<T, Function>(f), _1, boost::ref(*_coros.front().get()))));
	}
protected:
	std::stack<push_type_ptr<T> > _coros;
	std::stack<T> _buf;
	fes::semaphore _full;
	fes::semaphore _empty;
};

}

#endif
