#ifndef _CU_PARALLEL_SCHEDULER_H_
#define _CU_PARALLEL_SCHEDULER_H_

#include <teelogging/teelogging.h>
#include "cpproutine.h"
#include "scheduler.h"

namespace cu {

class sequence_scheduler : public scheduler {
public:
	virtual ~sequence_scheduler()
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
		while (i != _running.end())
		{
			auto& c = *i;
			if(c->ready())
			{
				_active = c.get();
				{
					_move_to_blocked = false;
					_last_id = -1;
					c->run();
					if (_move_to_blocked)
					{
						auto& blocked = _blocked[_last_id];
						blocked.emplace_back(std::move(c));
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
		return ready();
	}
};

}

#endif
