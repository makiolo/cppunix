#ifndef _CU_SEMAPHORE_H_
#define _CU_SEMAPHORE_H_

#include <teelogging/teelogging.h>
#include "scheduler.h"

namespace cu {

static int last_id = 0;

class semaphore
{
public:
	explicit semaphore(cu::scheduler& sche, int count_initial = 1)
		: _sche(sche)
		, _count(count_initial)
		, _id(last_id++)
	{
		LOGI("<%d> created semaphore %d", _id, _count);
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
	void notify(int q=1)
	{
		_count += q;
		LOGI("<%d> increase semaphore from %d to %d", _id, _count-q, _count);
		if(_count <= 0)
		{
			_sche.notify_one(_id);
		}
	}

	void notify(cu::push_type<control_type>& yield, int q=1)
	{
		_count += q;
		LOGI("<%d> increase semaphore from %d to %d", _id, _count-q, _count);
		if(_count <= 0)
		{
			if(_sche.notify_one(_id))
			{
				LOGI("notify yield in semaphore %d", _id);
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
	void wait(int q = 1)
	{
		_count -= q;
		// TODO: evitar bajar de 0
		LOGI("<%d> decrease semaphore from %d to %d", _id, _count+q, _count);
		if(_count < 0)
		{
			LOGI("wait no-yield in semaphore %d", _id);
			_sche.wait(_id);
		}
	}

	void wait(cu::push_type<control_type>& yield, int q = 1)
	{
		_count -= q;
		// TODO: evitar bajar de 0
		LOGI("<%d> decrease semaphore from %d to %d", _id, _count+q, _count);
		if(_count < 0)
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
};

}

#endif

