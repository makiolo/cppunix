#ifndef _CU_CHANNEL_H_
#define _CU_CHANNEL_H_

#include <iostream>
#include <vector>
#include <stack>
#include <boost/bind.hpp>
#include <coroutine/coroutine.h>
#include "semaphore.h"
#include <fast-event-system/sem.h>
#include <fast-event-system/async_delay.h>
#include <assert.h>
#include <mutex>

namespace cu {

template <typename T> class channel;
template <typename T> struct optional;

namespace detail {

	template <typename R>
	static auto _pipe(std::vector< typename cu::link< optional<R> > >& links, R input)
	{
		std::vector<R> output;
		std::vector< typename cu::channel<R>::generator > coros;
		coros.emplace_back( cu::make_generator< optional<R> >( [&](auto& yield) { yield(input); }) );
		for (auto& f : links)
		{
			coros.emplace_back( cu::make_generator< optional<R> >(boost::bind(f, boost::ref(*coros.back().get()), _1) ) );
		}
		auto pipeline = {
			[&output]() -> typename cu::channel<R>::link
			{
				return [&output](auto& source, auto& yield)
				{
					for (auto& s : source)
					{
						if(s)
						{
							output.emplace_back(*s);
						}
					}

				};
			}()
		};
		for (auto& f : pipeline)
		{
			coros.emplace_back( cu::make_generator< optional<R> >(boost::bind(f, boost::ref(*coros.back().get()), _1) ) );
		}
		return output;
	}
}

// problema: no es posible tener: channel<bool> por las ambiguedades en el constructor
template <typename T>
struct optional
{
	optional(T data) : _data(std::move(data)), _invalid(false) { ; }
	explicit optional(T&& data) : _data(data), _invalid(false) { ; }
	explicit optional() : _data(), _invalid(false) { ; }
	explicit optional(bool close) : _data(), _invalid(close) { ; }

	const T& operator*() const
	{
		return _data;
	}

	T& operator*()
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

template <typename T>
auto term_receiver(const typename channel<T>::coroutine& receiver)
{
	return [=](typename channel<T>::in& source)
	{
		for(auto& s : source)
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
	using generator = pull_type_ptr< optional<T> >;

	explicit channel(cu::scheduler& sch, size_t buffer = 0)
		: _sch(sch)
		, _buffer(buffer)
		, _elements(sch, 0)
		, _slots(sch, buffer + 1)
	{
		_set_tail();
	}

	template <typename Function>
	explicit channel(cu::scheduler& sch, size_t buffer, Function&& f)
		: _sch(sch)
		, _buffer(buffer)
		, _elements(sch, 0)
		, _slots(sch, buffer + 1)
	{
		_set_tail();
		_add(std::forward<Function>(f));
	}

	template <typename Function, typename ... Functions>
	explicit channel(cu::scheduler& sch, size_t buffer, Function&& f, Functions&& ... fs)
		: _sch(sch)
		, _buffer(buffer)
		, _elements(sch, 0)
		, _slots(sch, buffer + 1)
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

	template <typename R>
	void operator()(const R& data)
	{
		for(auto& e : pipe(T(data)))
		{
			_slots.wait();
			(*_coros.top())( optional<T>(e) );
			_elements.notify();
		}
	}

	template <typename R>
	void operator()(cu::yield_type& yield, const R& data)
	{
		for(auto& e : pipe(T(data)))
		{
			_slots.wait(yield);
			(*_coros.top())( optional<T>(e) );
			_elements.notify(yield);
			if(full())
			{
				yield();
			}
		}
	}

	void send_stdin()
	{
		for (std::string line; std::getline(std::cin, line);)
		{
			operator()<std::string>(line);
		}
	}

	void send_stdin(cu::yield_type& yield)
	{
		for (std::string line; std::getline(std::cin, line);)
		{
			operator()<std::string>(yield, line);
		}
	}

	optional<T> get()
	{
		_elements.wait();
		optional<T> data = std::get<0>(_buf.get());
		_slots.notify();
		return std::move(data);
	}

	optional<T> get(cu::yield_type& yield)
	{
		if(_buf.empty())
		{
			yield();
		}
		_elements.wait(yield);
		optional<T> data = std::get<0>(_buf.get(yield));
		_slots.notify(yield);
		// if(empty())
		// {
		// 	yield();
		// }
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
		_slots.wait();
		(*_coros.top())( optional<T>(true) );
		_elements.notify();
	}

	void close(cu::yield_type& yield)
	{
		_slots.wait(yield);
		(*_coros.top())( optional<T>(true) );
		_elements.notify(yield);
		yield();
	}

protected:

	template <typename R>
	auto pipe(const R& input)
	{
		return cu::detail::_pipe<R>(_links, input);
	}

	void _set_tail()
	{
		auto r = cu::make_iterator< optional<T> >(
			[this](auto& source) {
				for(auto& s : source)
				{
					this->_buf(0, fes::deltatime(0), s);
				}
				
			}
		);
		_coros.push( cu::make_iterator< optional<T> >( term_receiver<T>(r) ) );
	}

	template <typename Function>
	void _add(Function&& f)
	{
		_links.emplace(_links.begin(), std::forward<Function>(f));
	}

	template <typename Function, typename ... Functions>
	void _add(Function&& f, Functions&& ... fs)
	{
		_add(std::forward<Functions>(fs)...);
		_links.emplace(_links.begin(), std::forward<Function>(f));
	}

protected:
	cu::scheduler& _sch;
	size_t _buffer;
	std::stack< coroutine > _coros;
	fes::async_delay< optional<T> > _buf;
	cu::semaphore _elements;
	cu::semaphore _slots;
	std::vector<link> _links;
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
inline int select_nonblock(cu::yield_type& yield, const cu::channel<Args>&... chans)
{
	return cu::_which(0, chans...);
}

template <typename... Args>
inline int select(cu::yield_type& yield, const cu::channel<Args>&... chans)
{
	int n;
	do
	{
		n = select_nonblock(yield, chans...);
		if(n == -1)
		{
			yield();
		}
	} while(n == -1);
	return n;
}

template <size_t N, typename T, typename ... STUFF>
bool _barrier(cu::yield_type& yield, cu::optional< std::tuple<STUFF...> >& tpl, cu::channel<T>& chan)
{
	cu::optional<T> a;
	if(cu::select(yield, chan) == 0)
	{
		a = chan.get(yield);
		if(a)
			std::get<N>(*tpl) = *a;
		else
			return false;
	}
	return true;
}

template <size_t N, typename T, typename ... Args, typename ... STUFF>
bool _barrier(cu::yield_type& yield, cu::optional< std::tuple<STUFF...> >& tpl, cu::channel<T>& chan, cu::channel<Args>&... chans)
{
	cu::optional<T> a;
	if(cu::select(yield, chan) == 0)
	{
		a = chan.get(yield);
		if(a)
			std::get<N>(*tpl) = *a;
		else
			return false;
	}
	return _barrier<N+1>(yield, tpl, chans...);
}

template <typename ... Args>
cu::optional< std::tuple<Args...> > barrier(cu::yield_type& yield, cu::channel<Args>&... chans)
{
	cu::optional< std::tuple<Args...> > tpl(false);
	bool ok = _barrier<0>(yield, tpl, chans...);
	if(!ok)
	{
	    return cu::optional< std::tuple<Args...> >(true);
	}
	return tpl;
}

template <typename ... Args>
auto range(cu::yield_type& yield, cu::channel<Args>&... chans)
{
	return cu::pull_type< std::tuple<Args...> >(
		[&](cu::push_type< std::tuple<Args...> >& own_yield) {
			for(;;)
			{
				auto data = cu::barrier(yield, chans...);
				if(data)
					own_yield(*data);
				else
					break; // detect close or exception
			}
		}
	);
}

template <typename T>
auto range(cu::yield_type& yield, cu::channel<T>& chan)
{
	return cu::pull_type<T>(
		[&](cu::push_type<T>& own_yield) {
			for(;;)
			{
				auto data = chan.get(yield);
				if(data)
					own_yield(*data);
				else
					break; // detect close or exception
			}
		}
	);
};

}

#endif

