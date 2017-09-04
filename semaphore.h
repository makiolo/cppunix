#ifndef _CU_SEMAPHORE_H_
#define _CU_SEMAPHORE_H_

#include <teelogging/teelogging.h>
#include "scheduler.h"

namespace cu {

static int last_id = 0;

class semaphore
{
public:
	explicit semaphore(cu::scheduler& sche, int count_initial = 0)
		: _sche(sche)
		, _count(count_initial)
		, _id(last_id++)
	{
		LOGV("<%d> created semaphore %d", _id, _count);
	}

	//!	avisar / signal / unlock / up / wakeup / release / V
	void notify()
	{
		++_count;
		LOGV("<%d> increase semaphore from %d to %d", _id, _count-1, _count);
		if(_count <= 0)
		{
			_sche.notify_one(_id);
		}
	}

	void notify(cu::yield_type& yield)
	{
		++_count;
		LOGV("<%d> increase semaphore from %d to %d", _id, _count-1, _count);
		if(_count <= 0)
		{
			if(_sche.notify_one(_id))
			{
				LOGV("notify yield in semaphore %d", _id);
				yield();
			}
		}
	}

	//! esperar / wait / lock / down / sleep / P
	void wait()
	{
		--_count;
		LOGV("<%d> decrease semaphore from %d to %d", _id, _count+1, _count);
		if(_count < 0)
		{
			LOGV("wait no-yield in semaphore %d", _id);
			_sche.wait(_id);
		}
	}

	void wait(cu::yield_type& yield)
	{
		--_count;
		LOGV("<%d> decrease semaphore from %d to %d", _id, _count+1, _count);
		if(_count < 0)
		{
			_sche.wait(_id);
			LOGV("wait yield in semaphore %d", _id);
			yield();
		}
	}

	inline bool empty() const
	{
		return (_count <= 0);
	}
	
	inline int size() const
	{
		return _count;
	}
	
	cu::scheduler& _sche;
	int _count;
	int _id;
};

}

#endif
