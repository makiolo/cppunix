#ifndef _CU_CPROUTINE_H_
#define _CU_CPROUTINE_H_

#include "coroutine.h"

namespace cu {

using control_type = void;

class cpproutine
{
public:
	template <typename Function>
	cpproutine(const std::string& name, int pid, Function&& func)
		: _name(name)
		, _pid(pid)
		, _coroutine(cu::make_generator<control_type>(
			[f = std::move(func)](auto& yield) {
				yield();
				f(yield);
			}
		))
	{
		
	}

	std::string get_name() const {return _name;}

	bool ready() const
	{
		return bool(*_coroutine);
	}

	void run()
	{
		(*_coroutine)();
	}
	
	int getpid() const
	{
		return _pid;
	}

protected:
	std::string _name;
	int _pid;
	cu::pull_type_ptr<control_type> _coroutine;
};

}

#endif

