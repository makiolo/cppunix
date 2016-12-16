#ifndef _CU_CHANNEL_H_
#define _CU_CHANNEL_H_

#include <deque>
#include <boost/bind.hpp>
#include "coroutine.h"

namespace cu {

template <typename T, typename Function>
channel<T>::link link(Function&& func)
{
	return [](channel<T>::in& source, channel<T>::out& yield)
	{
		for (auto& s : source)
		{
			func(s);
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

	template <typename Function>
	void connect(Function&& f)
	{
		_add(std::forward<Function>(f));
	}

	void operator()(const T& data)
	{
		(*_coros.front())(data);
	}

protected:
	void _set_tail()
	{
		_coros.emplace_front(cu::make_iterator<T>([](auto& source) { for(auto& v: source) { ; }; }));
	}

	template <typename Function>
	void _add(Function&& f)
	{
		_coros.emplace_front(cu::make_iterator<T>(boost::bind(link<T, Function>(f), _1, boost::ref(*_coros.front().get()))));
	}

	template <typename Function, typename ... Functions>
	void _add(Function&& f, Functions&& ... fs)
	{
		_add(std::forward<Functions>(fs)...);
		_coros.emplace_front(cu::make_iterator<T>(boost::bind<T, Function>(link(f), _1, boost::ref(*_coros.front().get()))));
	}
protected:
	std::deque<push_type_ptr<T> > _coros;
};

}

#endif
