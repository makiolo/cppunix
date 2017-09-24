#ifndef _CU_PARALLEL_SCHEDULER_H_
#define _CU_PARALLEL_SCHEDULER_H_

// #include <map>
#include <teelogging/teelogging.h>
// #include <asyncply/run.h>
// #include <asyncply/algorithm.h>
#include <fast-event-system/sync.h>
#include <mqtt/async_client.h>
#include "cpproutine.h"
#include "scheduler.h"

namespace cu {

class parallel_scheduler : public scheduler {
public:

	virtual ~parallel_scheduler()
	{
		;
	}

	virtual bool ready() const
	{
		return _running.size() > 0;
	}

	bool run() override final
	{
		auto i = _running.begin();
		LOGV("begin parallel_scheduler");
		while (i != _running.end())
		{
			auto& c = *i;
			if(c->ready())
			{
				_active = c.get();
				{
					_move_to_blocked = false;
					_last_id = -1;
					LOGV("<%s> begin run()", get_name().c_str());
					c->run();
					LOGV("<%s> end run()", get_name().c_str());

					if (_move_to_blocked)
					{
						LOGV("<%s> begin blocking", get_name().c_str());
						LOGV("%s: se bloquea, para esperar a la señal: %d", get_name().c_str(), _last_id);
						auto& blocked = _blocked[_last_id];
						blocked.emplace_back(std::move(c));
						i = _running.erase(i);
						LOGV("<%s> end blocking", get_name().c_str());
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
				LOGV("cpproutine ha terminado");
				i = _running.erase(i);
			}
		}
		LOGV("end parallel_scheduler");
		return _running.size() > 0;
	}
};

}

#endif

