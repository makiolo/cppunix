#ifndef _CU_CHANNEL_H_
#define _CU_CHANNEL_H_

#include <vector>
#include <stack>
#include <boost/bind.hpp>
#include "coroutine.h"
#include <fast-event-system/sem.h>
#include <fast-event-system/async_fast.h>
#include <assert.h>
#include <mutex>

namespace cu {
/*
class cpproutine
{
public:
};

class semaphore;

class scheduler
{
public:
	void lock(const semaphore& sem)
	{
		// cid id = whoiam_cpproutine()
		// if(is.is_running())
		// {
		//	move id to blocked
		// }
	}
protected:
	// std::vector<cpproutine> _starting;
	std::vector<cpproutine> _running;
	std::vector<cpproutine> _blocked;
	// std::vector<cpproutine> _ending;
};

class semaphore
{
public:
	explicit semaphore(scheduler& sche, int count = 0, int count_max = 1)
		: _sche(sche)
		, _count(count)
		, _count_max(count_max)
	{
		assert((1 <= count_max) || (0 <= count));
		assert(count <= count_max);
	}

	///
	/// Reduce el valor del semaforo. Bloquea la regi�n critica. Esta operaci�n tiene m�ltiples
	/// nombres.
	///  * wait (s)
	///	 * {
	///		  if s > 0
	///				s--
	///		  else // s == 0
	///				bloqueo
	///		}
	///
	///		esperar / wait / lock / down / sleep / P
	///
	inline void lock()
	{
		if(_count > 0)
		{
			--_count;
		}
		else
		{
			_sche.lock(*this);
		}
	}
	///
	/// Aumenta el semaforo. Libera la region critica.
	///    signal(s)
	///    {
	///        if s == 0
	///            s++
	///        else // s > 0
	///            if s < MAX
	///                s++
	///    }
	///
	///	avisar / signal / unlock / up / wakeup / release / V
	///
	inline void unlock()
	{
		if((_count == 0) || (_count < count_max))
		{
			++_count;
		}
	}
protected:
	scheduler& _sche;
	int _count;
	int _count_max;
}
*/

//
// rename optional<T>
//
template <typename T>
struct optional
{
	explicit optional() : _data(), _invalid(false) { ; }
	explicit optional(bool close) : _data(), _invalid(close) { ; }
	optional(const T& data) : _data(data), _invalid(false) { ; }

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

	channel(size_t buffer = 1)
		: _closed(false)
	{
		_set_tail(buffer);
	}

	template <typename Function>
	channel(size_t buffer, Function&& f)
		: _closed(false)
	{
		_set_tail(buffer);
		_add(std::forward<Function>(f));
	}

	template <typename Function, typename ... Functions>
	channel(size_t buffer, Function&& f, Functions&& ... fs)
		: _closed(false)
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

	template <typename R>
	void operator()(const R& data)
	{
		if(!_closed)
		{
			// std::unique_lock<std::mutex> lock(_w_coros);
			_empty.wait();
			(*_coros.top())( optional<T>(data) );
			_full.notify();
		}
	}

	channel<T>& operator<<(const T& data)
	{
		operator()<T>(data);
		return *this;
	}

	optional<T> get()
	{
		optional<T> data;
		operator>>(data);
		return data;
	}

	optional<T>& operator>>(optional<T>& data)
	{
		_full.wait();
		data = std::get<0>( _buf.get() );
		_empty.notify();
		return data;
	}

	void close()
	{
		operator()<bool>(true);
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
		assert(buffer > 0);
		auto r = cu::make_iterator< optional<T> >(
			[this](auto& source) {
				for (auto& s : source)
				{
					this->_buf(s);
				}
			}
		);
		_coros.push( cu::make_iterator< optional<T> >( term_receiver<T>(r) ) );

		for(size_t i = 0;i < buffer; ++i)
			_empty.notify();
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
	fes::semaphore _full;
	fes::semaphore _empty;
	// std::mutex _w_coros;
	bool _closed;
};

}

#endif

