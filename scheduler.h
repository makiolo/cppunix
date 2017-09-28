#ifndef _CU_SCHEDULER_H_
#define _CU_SCHEDULER_H_

#include <map>
#include <teelogging/teelogging.h>
#include "cpproutine.h"
#include <asyncply/run.h>
#include <asyncply/algorithm.h>
#include <fast-event-system/sync.h>
#include <mqtt/async_client.h>

namespace asyncply
{
	template <typename TOKEN>
	struct is_complete
	{
		bool operator()(TOKEN token) const
		{
			return token->is_ready();
		}
	};

	template <>
	struct is_complete<mqtt::token_ptr>
	{
		bool operator()(mqtt::token_ptr token) const
		{
			return token->is_complete();
		}
	};

	template <>
	struct is_complete<mqtt::delivery_token_ptr>
	{
		bool operator()(mqtt::delivery_token_ptr token) const
		{
			return token->is_complete();
		}
	};
}

namespace cu {

static void sleep(cu::yield_type& yield, fes::deltatime time)
{
	auto timeout = fes::high_resolution_clock() + time;
	while(fes::high_resolution_clock() <= timeout)
	{
		yield();
	}
}

template <typename TOKEN>
static void await(cu::yield_type& yield, TOKEN token)
{
	while(!asyncply::is_complete<TOKEN>{}(token))
	{
		yield();
	}
}

// implementar ejecutar corutina "atexit" al salir
class scheduler : public scheduler_basic
{
public:
	explicit scheduler()
		: _pid_counter(0)
		, _active(nullptr)
	{
		
	}

	virtual ~scheduler()
	{
		;
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
	
	void run_until_complete()
	{
		while(ready())
		{
			run();
		}

		// bool pending_work;
		// do
		// {
		// 	pending_work = run();
		// } while(pending_work);
		
		if(_blocked.size() > 0)
		{
			std::stringstream ss;
			ss << "fatal error: all cpproutines are asleep" << std::endl;
			throw std::runtime_error(ss.str());
		}
	}
	
	void run_forever()
	{
		while(true)
		{
			run();
		}
	}

	std::string get_name() const override final
	{
		return _active->get_name();
	}
	
	pid_type getpid() const override final
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
			LOGV("<%s> begin resuming", get_name().c_str());
			LOGV("%s se desbloquea porque ha sido despertado por la señal %d", (*blocked.begin())->get_name().c_str(), id);
			_running.emplace(_running.end(), std::move(*blocked.begin()));
			blocked.erase(blocked.begin());
			if(blocked.size() == 0)
			{
				_blocked.erase(id);
			}
			LOGV("<%s> end resuming", get_name().c_str());
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
			LOGV("<%s> begin resuming", get_name().c_str());
			LOGV("%s se desbloquea porque ha sido despertado por la señal %d", (*blocked.begin())->get_name().c_str(), id);
			_running.emplace(_running.end(), std::move(*blocked.begin()));
			blocked.erase(blocked.begin());
			if(blocked.size() == 0)
			{
				_blocked.erase(id);
			}
			notified_any = true;
			LOGV("<%s> end resuming", get_name().c_str());
		}
		return notified_any;
	}
	
protected:
	scheduler_basic* _active;
	// normal running
	std::vector<std::unique_ptr<scheduler_basic> > _running;
	// cpproutines waiting for pid
	std::map<int, std::vector<std::unique_ptr<scheduler_basic> > > _blocked;
	pid_type _pid_counter;
	bool _move_to_blocked;
	int _last_id;
};

}

#endif
