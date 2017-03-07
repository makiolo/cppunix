#ifndef _CU_SEMAPHORE_H_
#define _CU_SEMAPHORE_H_

#include <teelogging/teelogging.h>
#include "scheduler.h"

namespace cu {

static int last_id = 0;

class semaphore
{
public:
	explicit semaphore(cu::scheduler& sche, int count_max = 1, int count_initial = 0)
		: _sche(sche)
		, _count(count_initial)
		, _count_max(count_max)
		, _id(last_id++)
	{
		assert((1 <= count_max) || (0 <= count_initial));
		assert(count_initial <= count_max);
		LOGI("<%d> created semaphore %d / %d", _id, _count, _count_max);
	}
	///
	/// Aumenta el semaforo. Libera la region critica.
	///    signal(s)
	///    {
	///        if s == 0
	///            s++
	///        else // s > 0
	///            if s < MAX
	///                s++
	///    }
	///
	///	avisar / signal / unlock / up / wakeup / release / V
	///
	void notify()
	{
		if((_count == 0) || (_count < _count_max))
		{
			++_count;
			LOGI("<%d> increase semaphore from %d to %d / %d", _id, _count-1, _count, _count_max);
		}
		
		if(_count >= _count_max)
		{
			LOGI("notify semaphore %d is full with %d", _id, _count);
			_sche.notify_all(_id);
		}
	}

	void notify(cu::push_type<control_type>& yield)
	{
		if((_count == 0) || (_count < _count_max))
		{
			++_count;
			LOGI("<%d> increase semaphore from %d to %d / %d", _id, _count-1, _count, _count_max);
		}
		
		if(_count >= _count_max)
		{
			LOGI("notify semaphore %d is full with %d", _id, _count);
			if(_sche.notify_all(id))
			{
				LOGI("notify yield in semaphore %d", id);
				yield();
			}
		}
	}

	///
	/// Reduce el valor del semaforo. Bloquea la regi�n critica. Esta operaci�n tiene m�ltiples
	/// nombres.
	///  * wait (s)
	///	 * {
	///		  if s > 0
	///				s--
	///		  else // s == 0
	///				bloqueo
	///		}
	///
	///		esperar / wait / lock / down / sleep / P
	///
	void wait()
	{
		if(_count > 0)
		{
			--_count;
			LOGI("<%d> decrease semaphore from %d to %d / %d", _id, _count+1, _count, _count_max);
		}
		else
		{
			LOGI("wait no-yield in semaphore %d", _id);
			_sche.wait(_id);
		}
	}

	void wait(cu::push_type<control_type>& yield)
	{
		if(_count > 0)
		{
			--_count;
			LOGI("<%d> decrease semaphore from %d to %d / %d", _id, _count+1, _count, _count_max);
		}
		else
		{
			_sche.wait(_id);
			LOGI("wait yield in semaphore %d", _id);
			yield();
		}
	}
protected:
	int _id;
	cu::scheduler& _sche;
	int _count;
	int _count_max;
};

}

#endif

