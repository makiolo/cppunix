#include <atomic>
#include <string>
#include <gtest/gtest.h>
#include <teelogging/teelogging.h>
#include "../shell.h"
#include "../coroutine.h"
#include "../scheduler.h"
#include "../semaphore.h"
#include "../channel.h"

class CoroTest : testing::Test { };

using namespace cu;

TEST(CoroTest, Test_find)
{
	cu::scheduler sch;

	LOGI("---------- begin find test --------");
	cu::channel<std::string> c1(sch, 20);
	c1.pipeline(	  find()
			, grep("*.h")
			, cat()
			, replace("class", "object")
			, log() );
	c1("../..");
	LOGI("---------- end find test --------");
}

TEST(CoroTest, Test_run_ls_strip_quote_grep)
{
	cu::scheduler sch;

	cu::channel<std::string> c1(sch, 100);
	c1.pipeline(
			  run()
			, strip()
			, quote()
			, grep("shell_*")
			, assert_count(1)
			, assert_string("\"shell_exe\"")
			, log()
	);
	c1("ls .");

	/////////////////////////////////

	cu::channel<std::string> c2(sch, 100);
	c2.pipeline(
			  ls()
			, strip()
			, quote()
			, grep("shell_*")
			, assert_count(1)
			, assert_string("\"shell_exe\"")
			, log()
	);
	c2(".");
}

TEST(CoroTest, Test_run_ls_sort_grep_uniq_join)
{
	cu::scheduler sch;

	cu::channel<std::string> c1(sch, 100);
	std::string out_subproces;
	c1.pipeline(run(), strip(), sort(), grep("*fes*"), uniq(), join(), log(), out(out_subproces));
	c1("ls .");
	//
	cu::channel<std::string> c2(sch, 100);
	std::string out_ls;
	c2.pipeline(ls(), sort(), grep("*fes*"), uniq(), join(), log(), out(out_ls));
	c2(".");
	//
	ASSERT_STREQ(out_subproces.c_str(), out_ls.c_str());
}

TEST(CoroTest, TestCut)
{
	cu::scheduler sch;

	cu::channel<std::string> c1(sch, 100);
	c1.pipeline(
			  assert_count(1)
			, split()
			, assert_count(3)
			, join()
			, assert_count(1)
			, cut(0)
			, assert_count(1)
			, assert_string("hello")
	);
	c1("hello big world");

	cu::channel<std::string> c2(sch, 100);
	c2.pipeline(
			  assert_count(1)
			, split()
			, assert_count(3)
			, join()
			, assert_count(1)
			, cut(1)
			, assert_count(1)
			, assert_string("big")
	);
	c2("hello big world");

	cu::channel<std::string> c3(sch, 100);
	c3.pipeline(
			  assert_count(1)
			, split()
			, assert_count(3)
			, join()
			, assert_count(1)
			, cut(2)
			, assert_count(1)
			, assert_string("world")
	);
	c3("hello big world");
}

TEST(CoroTest, TestGrep)
{
	cu::scheduler sch;

	cu::channel<std::string> c1(sch, 100);
	c1.pipeline(
			  split("\n")
			, assert_count(3)
			, grep("line2")
			, assert_count(1)
			, assert_string("line2")
	);
	c1("line1\nline2\nline3");
}

TEST(CoroTest, TestGrep2)
{
	cu::scheduler sch;

	cu::channel<std::string> c1(sch, 100);
	c1.pipeline(
			  split("\n")
			, assert_count(4)
	);
	c1("line1\nline2\nline3\n");
}

// TEST(CoroTest, TestCount)
// {
// 	cu::scheduler sch;
//
// 	cu::channel<std::string> c1(sch, 100);
// 	int result;
// 	c1.pipeline(
// 			  split("\n")
// 			, count()
// 			, out(result)
// 	);
// 	c1("line1\nline2\nline3");
// 	ASSERT_EQ(result, 3) << "maybe count() is not working well";
// }

TEST(CoroTest, TestUpper)
{
	cu::scheduler sch;
	cu::channel<std::string> c1(sch, 10);
	c1.pipeline( replace("mundo", "gente"), toupper() );
	sch.spawn([&](auto& yield) {
		for(int x=1; x<=1; ++x)
		{
			LOGI("sending hola mundo");
			c1(yield, "hola mundo");
		}
		c1.close(yield);
	});
	sch.spawn([&](auto& yield) {
		LOGI("begin");
		for(auto& r : cu::range(yield, c1))
		{
			LOGI("recv %s", r.c_str());
			// ASSERT_STREQ("HOLA GENTE", r.c_str());
		}
		LOGI("end");
	});
	sch.run_until_complete();
}

TEST(CoroTest, TestScheduler2)
{
	cu::scheduler sch;

	cu::channel<int> c1(sch, 10);
	cu::channel<int> c2(sch, 10);
	cu::channel<int> c3(sch, 10);

	c1.pipeline(
		[]() -> cu::channel<int>::link
		{
			return [](auto& source, auto& yield)
			{
				for (auto& s : source)
				{
					if(s)
						yield(*s + 1);
					else
						yield(s);
				}
			};
		}()
	);
	c2.pipeline(
		[]() -> cu::channel<int>::link
		{
			return [](auto& source, auto& yield)
			{
				for (auto& s : source)
				{
					if(s)
						yield(*s - 1);
					else
						yield(s);
				}
			};
		}()
	);
	c3.pipeline(
		[]() -> cu::channel<int>::link
		{
			return [](auto& source, auto& yield)
			{
				int total = 0;
				int count = 0;
				for (auto& s : source)
				{
					if(s)
					{
						total += *s;
						++count;
					}
					else
						yield(s);
				}
				LOGI("---> media es %d", total / count);
				yield( int(total / count) );
			};
		}()
	);	
	
	sch.spawn([&](auto& yield)
	{
		for(int x=1; x<=50; ++x)
		{
			LOGI("1. send %d", x);
			c1(yield, x);
		}
		c1.close(yield);
	});
	sch.spawn([&](auto& yield)
	{
		for(int y=1; y<=50; ++y)
		{
			LOGI("2. send %d", y);
			c2(yield, y);
		}
		c2.close(yield);
	});
	sch.spawn([&](auto& yield)
	{
		int a, b;
		for(auto& t : cu::range(yield, c1, c2))
		{
			std::tie(a, b) = t;
			LOGI("3. recv and resend %d", a+b);
			c3(yield, a + b);
		}
		c3.close(yield);
	});
	sch.spawn([&](auto& yield)
	{
		for(auto& r : cu::range(yield, c3))
		{
			LOGI("4. result = %d", r);
		}
	});
	sch.run_until_complete();
}

TEST(CoroTest, Test_Finite_Machine_States)
{
	cu::scheduler sch;

	cu::channel<int> sensor_cerca(sch, 10);
	cu::channel<int> sensor_lejos(sch, 10);
	cu::channel<int> action_hablarle(sch, 10);
	cu::channel<int> action_gritarle(sch, 10);
	
	sch.spawn([&](auto& yield)
	{
		// sensor code
		//for(;;)
		{
			// check sensor
			sensor_cerca(yield, 1);
			sensor_cerca(yield, 1);
			sensor_cerca(yield, 1);
			sensor_cerca(yield, 1);
			sensor_cerca(yield, 1);
			sensor_lejos(yield, 1);
			sensor_lejos(yield, 1);
			sensor_lejos(yield, 1);
			sensor_lejos(yield, 1);
			sensor_cerca(yield, 1);
			sensor_cerca(yield, 1);
		}
	});
	sch.spawn([&](auto& yield)
	{
		// finite machine state
		// TODO: can generate this code with metaprogramation ?
		// proposal:
		// cu::fsm(yield, 	sensor_cerca, action_hablarle,
		// 			sensor_lejos, action_gritarle);
		int state = -1;
		int prev_state = -1;
		for(;;)
		{
			prev_state = state;
			state = cu::select(yield, sensor_cerca, sensor_lejos);
			if(prev_state == state)
			{
				continue;
			}
			else
			{
				// prev_state -> onFinish() ?
				// state -> onStart() ?
			}
			switch(state)
			{
				case 0: // cerca
				{
					auto data = sensor_cerca.get(yield);
					if(data)
						action_hablarle(yield, *data);
				}
				break;
				case 1: // lejos
				{
					auto data = sensor_lejos.get(yield);
					if(data)
						action_gritarle(yield, *data);
				}
				break;
				default:
			}
		}
	});
	sch.spawn([&](auto& yield)
	{
		// action hablarle
		for(auto& e : cu::range(yield, action_hablarle))
		{
			std::cout << "hablando ..." << std::endl;
		}
	});
	sch.spawn([&](auto& yield)
	{
		// action gritarle
		for(auto& e : cu::range(yield, action_gritarle))
		{
			std::cout << "gritando ..." << std::endl;
		}
	});
	// run in slides
	sch.run();
	sch.run();
	sch.run();
	sch.run();
	sch.run();
	sch.run();
}
