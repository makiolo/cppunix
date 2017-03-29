#ifndef _CU_CHANNEL_H_
#define _CU_CHANNEL_H_

#include <iostream>
#include <vector>
#include <stack>
#include <boost/bind.hpp>
#include "coroutine.h"
#include "semaphore.h"
#include <fast-event-system/sem.h>
#include <fast-event-system/async_fast.h>
#include <assert.h>
#include <mutex>

namespace cu {

template <typename T>
struct optional
{
	optional(const T& data) : _data(data), _invalid(false) { ; }
	explicit optional() : _data(), _invalid(false) { ; }
	explicit optional(bool close) : _data(), _invalid(close) { ; }
	explicit optional(T&& data) : _data(std::move(data)), _invalid(false) { ; }

	const T& operator*() const
	{
		return _data;
	}

	operator bool() const
	{
		return !_invalid;
	}

	T _data;
	bool _invalid;
};

template <typename T> class channel;

template <typename T>
auto term_receiver(const typename channel<T>::coroutine& receiver)
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
	using in = cu::pull_type< optional<T> >;
	using out = cu::push_type< optional<T> >;
	using link = cu::link< optional<T> >;
	using coroutine = push_type_ptr< optional<T> >;

	explicit channel(cu::scheduler& sch, size_t buffer = 0)
		: _elements(sch, 0)
		, _slots(sch, buffer + 2)
		, _buf(buffer + 2)
	{
		_set_tail();
	}

	template <typename Function>
	explicit channel(cu::scheduler& sch, size_t buffer, Function&& f)
		: _elements(sch, 0)
		, _slots(sch, buffer + 2)
		, _buf(buffer + 2)
	{
		_set_tail();
		_add(std::forward<Function>(f));
	}

	template <typename Function, typename ... Functions>
	explicit channel(cu::scheduler& sch, size_t buffer, Function&& f, Functions&& ... fs)
		: _elements(sch, 0)
		, _slots(sch, buffer + 2)
		, _buf(buffer + 2)
	{
		_set_tail();
		_add(std::forward<Function>(f), std::forward<Functions>(fs)...);
	}
	
	template <typename Function>
	void pipeline(Function&& f)
	{
		_add(std::forward<Function>(f));
	}

	template <typename Function, typename ... Functions>
	void pipeline(Function&& f, Functions&& ... fs)
	{
		_add(std::forward<Function>(f), std::forward<Functions>(fs)...);
	}

	// void pop()
	// {
	// 	_coros.pop();
	// }

	template <typename R>
	void operator()(const R& data)
	{
		// producer
		// std::unique_lock<std::mutex> lock(_w_coros);
		_slots.wait();
		// _mutex.wait();
		(*_coros.top())( optional<T>(data) );
		// _mutex.notify();
		_elements.notify();
	}

	// producer
	template <typename R>
	void operator()(cu::push_type<control_type>& yield, const R& data)
	{
		// producer
		// std::unique_lock<std::mutex> lock(_w_coros);
		_slots.wait(yield);
		// _mutex.wait();
		(*_coros.top())( optional<T>(data) );
		// _mutex.notify();
		_elements.notify(yield);
		// TODO: revisar esto
		if(full())
		{
			yield();
		}
	}
	
	void send_stdin()
	{
		// TODO: if (!isatty(fileno(stdin)))
		
		for (std::string line; std::getline(std::cin, line);)
		{
			operator()<std::string>(line);
		}
	}
	
	void send_stdin(cu::push_type<control_type>& yield)
	{
		// TODO: if (!isatty(fileno(stdin)))
		
		for (std::string line; std::getline(std::cin, line);)
		{
			operator()<std::string>(yield, line);
		}
	}

	optional<T> get()
	{
		// std::unique_lock<std::mutex> lock(_w_coros);
		_elements.wait();
		// _mutex.wait();
		optional<T> data = std::get<0>(_buf.get());
		// _mutex.notify();
		_slots.notify();
		return std::move(data);
	}

	// consumer
	optional<T> get(cu::push_type<control_type>& yield)
	{
		// std::unique_lock<std::mutex> lock(_w_coros);
		_elements.wait(yield);
		// _mutex.wait();
		optional<T> data = std::get<0>(_buf.get());
		// _mutex.notify();
		_slots.notify(yield);
		if(empty())
		{
			yield();
		}
		return std::move(data);
	}
	
	inline bool empty() const
	{
		return (_elements.size() <= 0);
	}
	
	inline bool full() const
	{
		return (_slots.size() <= 0);
	}

	void close()
	{
		operator()<bool>(true);
	}

	void close(cu::push_type<control_type>& yield)
	{
		operator()<bool>(yield, true);
	}

	template <typename Function>
	void for_each(cu::push_type<control_type>& yield, Function&& f)
	{
		for(;;)
		{
			auto data = get(yield);
 			if(data)
 				f(*data);
 			else
 				break; // detect close or exception
		}
	}

	template <typename Function>
	static void for_each(const std::vector< cu::channel<T> >& channels, cu::push_type<control_type>& yield, Function&& f)
	{
		/*
		for(;;)
		{
			auto data = get(yield);
 			if(data)
 				f(*data);
 			else
 				break; // detect close or exception
		}
		*/
	}

protected:
	void _set_tail()
	{
		// std::unique_lock<std::mutex> lock(_w_coros);
		auto r = cu::make_iterator< optional<T> >(
			[this](auto& source) {
				for (auto& s : source)
				{
					this->_buf(s);
				}
			}
		);
		_coros.push( cu::make_iterator< optional<T> >( term_receiver<T>(r) ) );
	}

	template <typename Function>
	void _add(Function&& f)
	{
		// std::unique_lock<std::mutex> lock(_w_coros);
		_coros.push(cu::make_iterator< optional<T> >(boost::bind(f, _1, boost::ref(*_coros.top().get()))));
	}

	template <typename Function, typename ... Functions>
	void _add(Function&& f, Functions&& ... fs)
	{
		// std::unique_lock<std::mutex> lock(_w_coros);
		_add(std::forward<Functions>(fs)...);
		_coros.push(cu::make_iterator< optional<T> >(boost::bind(f, _1, boost::ref(*_coros.top().get()))));
	}
protected:
	std::stack< coroutine > _coros;
	fes::async_fast< optional<T> > _buf;
	cu::semaphore _elements;
	cu::semaphore _slots;
	// fes::semaphore _mutex;
	// std::mutex _mutex;
};

template <typename T>
inline int _which(int n, const cu::channel<T>& chan)
{
	if (chan.empty())
		return -1;
	else
		return n;
}

template <typename T, typename... Args>
inline int _which(int n, const cu::channel<T>& chan, const cu::channel<Args>&... chans)
{
	if (chan.empty())
		return cu::_which(n + 1, chans...);
	else
		return n;
}

template <typename... Args>
inline int select_nonblock(cu::push_type<control_type>& yield, const cu::channel<Args>&... chans)
{
	int n = cu::_which(0, chans...);
	if(n == -1)
		yield();
	return n;
}

template <typename... Args>
inline int select(cu::push_type<control_type>& yield, const cu::channel<Args>&... chans)
{
	// block version
	int n;
	do
	{
		n = select_nonblock(yield, chans...);
	} while(n == -1);
	return n;
}

}

#endif
