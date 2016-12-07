#ifndef _CU_PIPELINE_H_
#define _CU_PIPELINE_H_

#include <vector>
#include <boost/bind.hpp>
#include "coroutine.h"

namespace cu {

template <typename T>
class pipeline
{
public:
	using in = cu::pull_type<T>;
	using out = cu::push_type<T>;
	using link = cu::link<T>;

	template <typename Function>
	pipeline(Function&& f)
	{
		std::vector<pull_type_ptr<T> > coros;
		coros.emplace_back(cu::make_generator<T>( [](auto& yield) { ; } ));
		coros.emplace_back(cu::make_generator<T>(boost::bind(f, boost::ref(*coros.back().get()), _1)));
		coros.emplace_back(cu::make_generator<T>(boost::bind(end_link<T>(), boost::ref(*coros.back().get()), _1)));
	}

	template <typename Function, typename ... Functions>
	pipeline(Function&& f, Functions&& ... fs)
	{
		std::vector<pull_type_ptr<T> > coros;
		coros.emplace_back(cu::make_generator<T>([](auto& yield) { ; }));
		coros.emplace_back(cu::make_generator<T>(boost::bind(f, boost::ref(*coros.back().get()), _1)));
		_add(coros, std::forward<Functions>(fs)...);
		coros.emplace_back(cu::make_generator<T>(boost::bind(end_link<T>(), boost::ref(*coros.back().get()), _1)));
	}

protected:
	template <typename Function>
	void _add(std::vector<pull_type_ptr<T> >& coros, Function&& f)
	{
		coros.emplace_back(cu::make_generator<T>(boost::bind(f, boost::ref(*coros.back().get()), _1)));
	}

	template <typename Function, typename ... Functions>
	void _add(std::vector<pull_type_ptr<T> >& coros, Function&& f, Functions&& ... fs)
	{
		coros.emplace_back(cu::make_generator<T>(boost::bind(f, boost::ref(*coros.back().get()), _1)));
		_add(coros, std::forward<Functions>(fs)...);
	}
};

}

#endif

