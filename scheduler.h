#ifndef _CU_SCHEDULER_H_
#define _CU_SCHEDULER_H_

#include <map>
#include <teelogging/teelogging.h>
#include "cpproutine.h"

namespace cu {

class scheduler {
public:
	scheduler()
		: _pid_counter(0)
		, _active(nullptr)
	{
		
	}

	template <typename Function>
	void spawn(const std::string& name, Function&& func)
	{
		_running.emplace_back(std::make_unique<cpproutine>(name, _pid_counter, std::forward<Function>(func)));
		++_pid_counter;
	}
		
	/*
	return true if any is updated
	*/
	bool run()
	{
		auto i = _running.begin();
		while (i != _running.end())
		{
			auto& c = *i;
			if(c->ready())
			{
				_active = c.get();
				{
					_move_to_blocked = false;
					c->run();

					if (_move_to_blocked)
					{
						LOGI("<%d> suspend process %s", getpid(), (c->get_name().c_str()));
						_blocked.emplace_back(std::move(c));
						i = _running.erase(i);
					}
					else
					{
						++i;
					}
				}
				_active = nullptr;
			}
			else
			{
				i = _running.erase(i);
			}
		}
		
		LOGD("still running %d", _running.size());
		LOGD("and blocked %d", _blocked.size());
		return _running.size() > 0;
	}
	
	void run_until_complete()
	{
		bool pending_work;
		do
		{
			pending_work = run();
		} while(pending_work);
	}
	
	void run_forever()
	{
		while(true)
		{
			run();
		}
	}
	
	pid_type getpid() const
	{
		return _active->getpid();
	}
	
	inline void wait(cu::push_type<control_type>& yield)
	{
		LOGD("lock pid %d", getpid());
		_move_to_blocked = true;
		yield();
	}

	inline void notify(cu::push_type<control_type>& yield)
	{
		if(_blocked.size() > 0)
		{
			LOGI("<%d> resume process %s", getpid(), (*_blocked.begin())->get_name().c_str());
			_running.emplace(_running.end(), std::move(*_blocked.begin()));
			_blocked.erase(_blocked.begin());
		}
		else
		{
			LOGD("nobody is waiting for %d", getpid());
		}
		yield();
	}
	
protected:
	cpproutine* _active;
	// normal running
	std::vector<std::unique_ptr<cpproutine> > _running;
	// cpproutines waiting for pid
	// std::map<int, std::vector<std::unique_ptr<cpproutine> > > _blocked;
	std::vector<std::unique_ptr<cpproutine> > _blocked;
private:
	pid_type _pid_counter;
	bool _move_to_blocked;
};

}

#endif

