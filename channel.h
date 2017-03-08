#ifndef _CU_CHANNEL_H_
#define _CU_CHANNEL_H_

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
	explicit optional() : _data(), _invalid(false) { ; }
	explicit optional(bool close) : _data(), _invalid(close) { ; }
	explicit optional(const T& data) : _data(data), _invalid(false) { ; }
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
struct channel_iterator
{
	channel_iterator(channel<T>& channel, bool is_begin=false)
		: _channel(channel)
		, _sentinel()
	{
		if(is_begin)
		{
			operator++();
		}
	}

	channel_iterator& operator=(const channel_iterator&)
	{
		return *this;
	}

	channel_iterator& operator++()
	{
		_sentinel = _channel.get();
	}

	T operator*() const
	{
		return *_sentinel;
	}

	template <typename Any>
	bool operator==(const Any&)
	{
		return !_sentinel;
	}

	template <typename Any>
	bool operator!=(const Any&)
	{
		return _sentinel;
	}
protected:
	channel<T>& _channel;
	optional<T> _sentinel;
};

template <typename T, typename Function>
typename channel<T>::link link_template(typename std::enable_if<(!std::is_void<typename std::result_of<Function(T)>::type>::value), Function>::type&& func)
{
	return [&func](typename channel<T>::in& source, typename channel<T>::out& yield)
	{
		for (auto& s : source)
		{
			if(s)
			{
				yield(optional<T>(func(*s)));
			}
			else
			{
				// skip func
				yield(s);
			}
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
			if(s)
			{
				func(*s);
			}
			yield(s);
		}
	};
}

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
		: _closed(false)
		, _buffer(buffer)
		, _elements(sch, 0)
		, _slots(sch, buffer)
	{
		_set_tail(buffer);
	}

	template <typename Function>
	explicit channel(cu::scheduler& sch, size_t buffer, Function&& f)
		: _closed(false)
		, _buffer(buffer)
		, _elements(sch, 0)
		, _slots(sch, buffer)
	{
		_set_tail(buffer);
		_add(std::forward<Function>(f));
	}

	template <typename Function, typename ... Functions>
	explicit channel(cu::scheduler& sch, size_t buffer, Function&& f, Functions&& ... fs)
		: _closed(false)
		, _buffer(buffer)
		, _elements(sch, 0)
		, _slots(sch, buffer)
	{
		_set_tail(buffer);
		_add(std::forward<Function>(f), std::forward<Functions>(fs)...);
	}

	template <typename Function>
	void connect(Function&& f)
	{
		_add(std::forward<Function>(f));
	}

	template <typename Function, typename ... Functions>
	void connect(Function&& f, Functions&& ... fs)
	{
		_add(std::forward<Function>(f), std::forward<Functions>(fs)...);
	}

	void pop()
	{
		_coros.pop();
	}

	/*
	// producer
	template <typename R>
	void operator()(const R& data)
	{
		// producer
		// std::unique_lock<std::mutex> lock(_w_coros);
		if(!_closed)
		{
			_slots.wait();
			(*_coros.top())( optional<T>(data) );
			_elements.notify();
		}
	}
	*/

	// producer
	template <typename R>
	void operator()(cu::push_type<control_type>& yield, const R& data)
	{
		// producer
		// std::unique_lock<std::mutex> lock(_w_coros);
		if(!_closed)
		{
			if (_buffer > 0)
				_slots.wait(yield);
			(*_coros.top())( optional<T>(data) );
			_elements.notify(yield);
		}
	}

	/*
	// consumer
	optional<T> get()
	{
		// std::unique_lock<std::mutex> lock(_w_coros);
		_elements.wait();
		optional<T> data = std::get<0>(_buf.get());
		_slots.notify();
		return std::move(data);
	}
	*/

	// consumer
	optional<T> get(cu::push_type<control_type>& yield)
	{
		// std::unique_lock<std::mutex> lock(_w_coros);
		_elements.wait(yield);
		optional<T> data = std::get<0>(_buf.get());
		if (_buffer > 0)
			_slots.notify(yield);
		return std::move(data);
	}

	void close()
	{
		operator()<bool>(true);
	}

	void close(cu::push_type<control_type>& yield)
	{
		operator()<bool>(yield, true);
	}

	auto begin()
	{
		return channel_iterator<T>(*this, true);
	}

	auto end()
	{
		return channel_iterator<T>(*this);
	}

protected:
	void _set_tail(size_t buffer)
	{
		// std::unique_lock<std::mutex> lock(_w_coros);
		// assert(buffer > 0);
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
		_coros.push(cu::make_iterator< optional<T> >(boost::bind(link_template<T, Function>(f), _1, boost::ref(*_coros.top().get()))));
	}

	template <typename Function, typename ... Functions>
	void _add(Function&& f, Functions&& ... fs)
	{
		// std::unique_lock<std::mutex> lock(_w_coros);
		_add(std::forward<Functions>(fs)...);
		_coros.push(cu::make_iterator< optional<T> >(boost::bind(link_template<T, Function>(f), _1, boost::ref(*_coros.top().get()))));
	}
protected:
	std::stack< coroutine > _coros;
	fes::async_fast< optional<T> > _buf;
	cu::semaphore _elements;
	cu::semaphore _slots;
	// std::mutex _w_coros;
	bool _closed;
	size_t _buffer;
};

}

#endif

