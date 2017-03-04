#ifndef _CU_SEMAPHORE_H_
#define _CU_SEMAPHORE_H_

#include <teelogging/teelogging.h>
#include "scheduler.h"

namespace cu {

class semaphore
{
public:
	explicit semaphore(cu::scheduler& sche, int count_max = 1, int count_initial = 0)
		: _sche(sche)
		, _count(count_initial)
		, _count_max(count_max)
	{
		assert((1 <= count_max) || (0 <= count_initial));
		assert(count_initial <= count_max);
		LOGI("created semaphore %d / %d", _count, _count_max);
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
	inline void notify(cu::push_type<control_type>& yield)
	{
		if((_count == 0) || (_count < _count_max))
		{
			++_count;
			_sche.notify(yield);
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
	inline void wait(cu::push_type<control_type>& yield)
	{
		if(_count > 0)
		{
			--_count;
		}
		else
		{
			_sche.wait(yield);
		}
	}
protected:
	cu::scheduler& _sche;
	int _count;
	int _count_max;
};

}

#endif

