#ifndef _CU_PARALLEL_SCHEDULER_H_
#define _CU_PARALLEL_SCHEDULER_H_

#include <teelogging/teelogging.h>
#include "cpproutine.h"
#include "scheduler.h"

namespace cu {

class parallel_scheduler : public scheduler {
public:

	explicit parallel_scheduler()
		: _ite( _running.begin() )
	{
		;
	}

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
		_ite = _running.begin();
		while (_ite != _running.end())
		{
			auto& c = *_ite;
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
						_ite = _running.erase(_ite);
					}
					else
					{
						++_ite;
					}
				}
				_active = nullptr;
			}
			else
			{
				_ite = _running.erase(_ite);
			}
		}
		return ready();
	}

protected:
	std::vector<std::unique_ptr<scheduler_basic> >::iterator _ite;
};

}

#endif
