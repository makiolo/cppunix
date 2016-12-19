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
typename channel<T>::link receiver_template(push_type_ptr<T>& receiver)
{
	return [&](typename channel<T>::in& source, typename channel<T>::out& yield)
	{
		for (auto& s : source)
		{
			(*receiver)(s);
			yield(s);
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
		_all_pull.wait();
		auto r = cu::make_iterator<T>(
			[&data](auto& source) {
				if(source)
				{
					std::cout << "source is ready" << std::endl;
				}
				for (auto& s : source)
				{
					std::cout << "RRRRRRRRRRREceived = " << s << std::endl;
				}
			}
		);
		_coros.emplace_back(cu::make_iterator<T>(boost::bind(receiver_template<T>(r), _1, boost::ref(*_coros.back().get()))));
		(*_coros.front())(data);
		_any_push.notify();
	}

	T& operator>>(T& data)
	{
		_any_push.wait();
		_coros.pop_back();
		_all_pull.notify();
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
		_coros.emplace_front(cu::make_iterator<T>([](auto& source) { for(auto& v: source) { ; }; }));
		// init all_pull is notified
		_all_pull.notify();
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
	std::deque<push_type_ptr<T> > _coros;
	fes::semaphore _any_push;
	fes::semaphore _all_pull;
};

}

#endif
