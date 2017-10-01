#ifndef _CU_CPROUTINE_H_
#define _CU_CPROUTINE_H_

#include <coroutine/coroutine.h>

namespace cu {

class scheduler_basic
{
public:
	virtual ~scheduler_basic() { ; }
	virtual bool run() = 0;
	virtual bool ready() const = 0;
	virtual std::string get_name() const = 0;
	virtual pid_type getpid() const = 0;

};

class cpproutine : public scheduler_basic
{
public:
	template <typename Function>
	explicit cpproutine(const std::string& name, pid_type pid, Function&& func)
		: _name(name)
		, _pid(pid)
		, _coroutine(cu::make_generator<control_type>(
			[f = std::move(func)](auto& yield) {
				yield( cu::control_type{} );
				f(yield);
			}
		))
	{ ; }

	virtual ~cpproutine() { ; }

	std::string get_name() const override final
	{
		return _name;
	}

	bool ready() const override final
	{
		return bool(*_coroutine);
	}

	bool run() override final
	{
		(*_coroutine)();
		return true;
	}
	
	int getpid() const override final
	{
		return _pid;
	}

protected:
	std::string _name;
	pid_type _pid;
	cu::pull_type_ptr<control_type> _coroutine;
};

}

#endif

