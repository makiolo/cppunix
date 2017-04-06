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

	cu::channel<int> c1(sch, 20);
	cu::channel<int> c2(sch, 20);
	cu::channel<int> c3(sch, 20);

	c1.pipeline(
		[]() -> cu::channel<int>::link
		{
			return [](auto& source, auto& yield)
			{
				int i = 0;
				for (auto& s : source)
				{
					// if(i % 2 == 0)
					{
						if(s)
							yield(*s);
						else
							yield(s);
					}
					++i;
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
						yield(*s);
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
				for(;;)
				{
					if(!source && (count > 0))
					{
						std::cout << "break!! " << (total / count) << std::endl;
						yield(total / count);
						break;
					}
					else
					{
						yield(999);
					}
					auto s = source.get();
					if(s)
					{
						total += *s;
						++count;
					}
					source();
				}
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

	cu::channel<float> sensor_cerca(sch, 10);
	cu::channel<float> sensor_lejos(sch, 10);
	//
	cu::channel<bool> on_change_hablarle(sch, 10);
	cu::channel<bool> on_change_gritarle(sch, 10);
	//
	cu::channel<float> update_hablarle(sch, 10);
	cu::channel<float> update_gritarle(sch, 10);

	sch.spawn([&](auto& yield)
	{
		// perception code (arduino)
		// auto x = enemy.get_position();
		// bool is_far = x.distance(me) > threshold;
		//for(;;)
		{
			// check sensor
			sensor_cerca(yield, 1.0);
			sensor_cerca(yield, 1.0);
			sensor_cerca(yield, 1.0);
			sensor_cerca(yield, 1.0);
			sensor_cerca(yield, 1.0);
			sensor_lejos(yield, 1.0);
			sensor_lejos(yield, 1.0);
			sensor_lejos(yield, 1.0);
			sensor_lejos(yield, 1.0);
			sensor_cerca(yield, 1.0);
			sensor_cerca(yield, 1.0);
		}
	});
	sch.spawn([&](auto& yield)
	{
		// decisions code (raspberry)
		// TODO: can generate this code with metaprogramation ?
		// proposal:
		// cu::fsm(yield, 	std::make_tuple(sensor_cerca, on_change_hablarle, update_hablarle),
		// 			std::make_tuple(sensor_lejos, on_change_gritarle, update_gritarle)
		//		);
		auto tpl = std::make_tuple(std::make_tuple(sensor_cerca, on_change_hablarle, update_hablarle),
					   std::make_tuple(sensor_lejos, on_change_gritarle, update_gritarle));
		auto& ref_tuple = std::get<0>(tpl);
		auto& sensor_prev = std::get<0>(ref_tuple);
		auto& change_prev = std::get<1>(ref_tuple);
		auto& update_prev = std::get<2>(ref_tuple);
		auto& sensor = std::get<0>(ref_tuple);
		auto& change = std::get<1>(ref_tuple);
		auto& update = std::get<2>(ref_tuple);
		int state = -1;
		int prev_state = -1;
		// TODO:
		// while(!sch.forcce_close())
		for(;;)
		{
			prev_state = state;
			sensor_prev = sensor;
			change_prev = change;
			update_prev = update;
			{
				auto& tuple_channels = std::get<0>(tpl);
				sensor = std::get<0>(tuple_channels);
				change = std::get<1>(tuple_channels);
				update = std::get<2>(tuple_channels);
				state = cu::select_nonblock(yield, sensor);
				if(state == 0)
				{
					auto data = sensor.get(yield);
					if(data)
					{
						state = state + 0;
						if(prev_state != state)
						{
							if(prev_state > -1)
								change_prev(yield, false);  // prev
							change(yield, true);  // state
						}
						update(yield, *data);
					}
				}
			}
			{
				auto& tuple_channels = std::get<1>(tpl);
				sensor = std::get<0>(tuple_channels);
				change = std::get<1>(tuple_channels);
				update = std::get<2>(tuple_channels);
				state = cu::select_nonblock(yield, sensor);
				if(state == 0)
				{
					auto data = sensor.get(yield);
					if(data)
					{
						state = state + 1;
						if(prev_state != state)
						{
							if(prev_state > -1)
								change_prev(yield, false);  // prev
							change(yield, true);  // state
						}
						update(yield, *data);
					}
				}
			}
		}
	});
	sch.spawn([&](auto& yield)
	{
		// action code
		//
		// while(!sch.forcce_close())
		for(;;)
		{
			/*
			// proposal
			cu::selector(yield, 
				     on_change_hablarle, [](auto& activation){
				     	;
				     }, 
				     update_hablarle, [](auto& value){
				     	std::cout << "hablando ..." << std::endl;
				     },
				     on_change_gritarle, [](auto& activation){
				     	;
				     }, 
				     update_gritarle, [](auto& value){
				     	std::cout << "gritando ..." << std::endl;
				     });
			*/
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
