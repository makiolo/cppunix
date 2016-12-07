#ifndef _CU_COROUTINE_H_
#define _CU_COROUTINE_H_

#include <boost/function.hpp>
#include <boost/coroutine2/coroutine.hpp>

namespace cu {

template <typename T>
using asymm_coroutine = boost::coroutines2::asymmetric_coroutine<T>;

template <typename T>
using pull_type = typename asymm_coroutine<T>::pull_type;

template <typename T>
using push_type = typename asymm_coroutine<T>::push_type;

template <typename T>
using pull_type_ptr = std::shared_ptr< pull_type<T> >;

template <typename T>
using push_type_ptr = std::shared_ptr< push_type<T> >;

template <typename T>
using link = boost::function<void(cu::pull_type<T>&, cu::push_type<T>&)>;

template <typename T, typename Function>
pull_type_ptr<T> make_generator(Function&& f)
{
	return std::make_shared< pull_type<T> >(std::forward<Function>(f));
}

template <typename T, typename Function>
push_type_ptr<T> make_iterator(Function&& f)
{
	return std::make_shared< push_type<T> >(std::forward<Function>(f));
}

template <typename T>
link<T> end_link()
{
	return [](cu::pull_type<T>& source, cu::push_type<T>&)
	{
		for (auto& s : source) { ; }
	};
}

}

#endif

