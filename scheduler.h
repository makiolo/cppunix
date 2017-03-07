#ifndef _CU_SCHEDULER_H_
#define _CU_SCHEDULER_H_

#include <map>
#include <teelogging/teelogging.h>
#include "cpproutine.h"

namespace cu {

class scheduler {
public:
	explicit scheduler()
		: _pid_counter(0)
		, _active(nullptr)
	{
		
	}

	template <typename Function>
	void spawn(Function&& func)
	{
		_running.emplace_back(std::make_unique<cpproutine>("anonymous", _pid_counter, std::forward<Function>(func)));
		++_pid_counter;
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
		LOGI("begin scheduler");
		while (i != _running.end())
		{
			auto& c = *i;
			if(c->ready())
			{
				_active = c.get();
				{
					_move_to_blocked = false;
					_last_id = -1;
					LOGI("<%s> begin run()", get_name().c_str());
					c->run();
					LOGI("<%s> end run()", get_name().c_str());

					if (_move_to_blocked)
					{
						LOGI("<%s> begin blocking", get_name().c_str());
						LOGI("%s: se bloquea, para esperar a la señal: %d", get_name().c_str(), _last_id);
						auto& blocked = _blocked[_last_id];
						blocked.emplace_back(std::move(c));
						i = _running.erase(i);
						LOGI("<%s> end blocking", get_name().c_str());
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
				LOGI("%s ha terminado", get_name().c_str());
				i = _running.erase(i);
			}
		}
		LOGI("end scheduler");
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

	std::string get_name() const
	{
		return _active->get_name();
	}
	
	pid_type getpid() const
	{
		return _active->getpid();
	}
	
	void wait(int id)
	{
		_move_to_blocked = true;
		_last_id = id;
	}

	bool notify_one(int id)
	{
		auto& blocked = _blocked[id];
		if(blocked.size() > 0)
		{
			LOGI("<%s> begin resuming", get_name().c_str());
			LOGI("%s se desbloquea porque ha sido despertado por la señal %d", (*blocked.begin())->get_name().c_str(), id);
			_running.emplace(_running.end(), std::move(*blocked.begin()));
			blocked.erase(blocked.begin());
			if(blocked.size() == 0)
			{
				_blocked.erase(id);
			}
			LOGI("<%s> end resuming", get_name().c_str());
			return true;
		}
		return  false;
	}
	
	bool notify_all(int id)
	{
		auto& blocked = _blocked[id];
		bool notified_any = false;
		while(blocked.size() > 0)
		{
			LOGI("<%s> begin resuming", get_name().c_str());
			LOGI("%s se desbloquea porque ha sido despertado por la señal %d", (*blocked.begin())->get_name().c_str(), id);
			_running.emplace(_running.end(), std::move(*blocked.begin()));
			blocked.erase(blocked.begin());
			if(blocked.size() == 0)
			{
				_blocked.erase(id);
			}
			notified_any = true;
			LOGI("<%s> end resuming", get_name().c_str());
		}
		return notified_any;
	}
	
protected:
	cpproutine* _active;
	// normal running
	std::vector<std::unique_ptr<cpproutine> > _running;
	// cpproutines waiting for pid
	std::map<int, std::vector<std::unique_ptr<cpproutine> > > _blocked;
private:
	pid_type _pid_counter;
	bool _move_to_blocked;
	int _last_id;
};

}

#endif

