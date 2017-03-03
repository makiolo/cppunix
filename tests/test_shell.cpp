#include <atomic>
#include "../shell.h"
#include "../coroutine.h"
#include <teelogging/teelogging.h>
#include <gtest/gtest.h>

class CoroTest : testing::Test { };

using namespace cu;

TEST(CoroTest, Test_run_ls_strip_quote_grep)
{
	cmd(
			  run("ls .")
			, strip()
			, quote()
			, grep("shell_exe")
			, assert_count(1)
			, assert_string("\"shell_exe\"")
	);

	cmd(
			  ls(".")
			, strip()
			, quote()
			, grep("shell_exe")
			, assert_count(1)
			, assert_string("\"shell_exe\"")
	);
}

TEST(CoroTest, Test_run_ls_sort_grep_uniq_join)
{
	std::string out_subproces;
	cmd(run("ls ."), strip(), sort(), grep_v("\\./."), grep_v("._exe"), uniq(), join(), out(out_subproces));
	//
	std::string out_ls;
	cmd(ls("."), sort(), grep_v("\\./."), grep_v("._exe"), uniq(), join(), out(out_ls));
	//
	ASSERT_STREQ(out_subproces.c_str(), out_ls.c_str());
}

TEST(CoroTest, TestCut)
{
	cmd(
			  in("hello big world")
			, assert_count(1)
			, split()
			, assert_count(3)
			, join()
			, assert_count(1)
			, cut(0)
			, assert_count(1)
			, assert_string("hello")
	);
	cmd(
			  in("hello big world")
			, assert_count(1)
			, split()
			, assert_count(3)
			, join()
			, assert_count(1)
			, cut(1)
			, assert_count(1)
			, assert_string("big")
	);
	cmd(
			  in("hello big world")
			, assert_count(1)
			, split()
			, assert_count(3)
			, join()
			, assert_count(1)
			, cut(2)
			, assert_count(1)
			, assert_string("world")
	);
}

TEST(CoroTest, TestGrep)
{
	cmd(	  
			  in("line1\nline2\nline3")
			, split("\n")
			, assert_count(3)
			, grep("line2")
			, assert_count(1)
			, assert_string("line2")
	);
}

TEST(CoroTest, TestGrep2)
{
	cmd(	  
			  in("line1\nline2\nline3\n")
			, split("\n")
			, assert_count(4)
	);
}

namespace cu {
	
class scheduler {
public:
	using control_type = void;
	
	template <typename Function>
	void spawn(Function&& func)
	{
		_running.emplace_back(cu::make_generator<control_type>(
			[f = std::move(func)](auto& yield) {
				yield();
				f(yield);
			}
		));
	}
		
	/*
	return true if any is updated
	*/
	bool run()
	{
		bool any_updated = false;
		_pid = 0;
		LOGD("total = %d", _running.size());
		for(auto& c : _running)
		{
			LOGD("ticking = %d", getpid());
			if(*c)
			{
				(*c)();
				any_updated = true;
			}
			++_pid;
		}
		return any_updated;
	}
	
	/*
	return true if have pending work
	*/
	// run_forever()
	// run_until_complete()
	void run_until_complete()
	{
		bool any_updated = true;
		while(any_updated)
		{
			any_updated = run();
		}
	}
	
	void run_forever()
	{
		while(true)
		{
			run();
		}
	}
	
	int getpid() const {return _pid;}
	
	inline void lock()
	{
		// cid id = whoiam_cpproutine()
		// if(is.is_running())
		// {
		//	move id to blocked
		// }
	}
	
	// auto create_semaphore();
	
	// each semaphore have 2 channels and ret code
	
protected:
	// normal running
	std::vector<cu::pull_type_ptr<control_type> > _running;
	// locked
	// std::vector<cu::pull_type_ptr<control_type> > _blocked;
	// unlocked and prepared for return
	// std::vector<cu::pull_type_ptr<control_type> > _ready;
private:
	int _pid;
};

class semaphore
{
public:
	explicit semaphore(scheduler& sche, int count = 0, int count_max = 1)
		: _sche(sche)
		, _count(count)
		, _count_max(count_max)
	{
		assert((1 <= count_max) || (0 <= count));
		assert(count <= count_max);
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
	inline void lock()
	{
		if(_count > 0)
		{
			--_count;
		}
		else
		{
			// bloquear esta cpproutine
			_sche.lock(); // (*this)
		}
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
	inline void unlock()
	{
		if((_count == 0) || (_count < _count_max))
		{
			++_count;
		}
		else
		{
			// ejecutar al primero que se bloqueo
		}
	}
protected:
	scheduler& _sche;
	int _count;
	int _count_max;
};

}

TEST(CoroTest, TestScheduler)
{
	const int N = 16;
	cu::scheduler sch;
	for(int i=0; i<N; ++i)
	{
		sch.spawn([&sch, i](auto& yield) {
			std::cout << "create " << i << " - pid: " << sch.getpid() << std::endl;
			yield();
			std::cout << "download " << i << " - pid: " << sch.getpid() << std::endl;
			yield();
			std::cout << "patching " << i << " - pid: " << sch.getpid() << std::endl;
			yield();
			std::cout << "compile " << i << " - pid: " << sch.getpid() << std::endl;
			yield();
			std::cout << "tests " << i << " - pid: " << sch.getpid() << std::endl;
			yield();
			std::cout << "packing " << i << " - pid: " << sch.getpid() << std::endl;
			yield();
			std::cout << "destroy " << i << " - pid: " << sch.getpid() << std::endl;
		});
	}
	sch.run_until_complete();
}

