#include <atomic>
#include <string>
#include <gtest/gtest.h>
#include <teelogging/teelogging.h>
#include <coroutine/coroutine.h>
#include "../shell.h"
#include "../scheduler.h"
#include "../semaphore.h"
#include "../channel.h"
#include <asyncply/run.h>
#include <mqtt/client.h>
#include <fast-event-system/async_fast.h>

class CoroTest : testing::Test { };

using namespace cu;


TEST(CoroTest, Test_find)
{
	cu::scheduler sch;
	cu::channel<std::string> c1(sch, 10);
	c1.pipeline(
			find(),
			grep("channel.h"),
			cat(),
			count()
	);
	sch.spawn([&](auto& yield) {
		for(int x=0; x<1; ++x)
		{
			c1(yield, "../..");
		}
		c1.close(yield);
	});
	sch.spawn([&](auto& yield) {
		for(auto& r : cu::range(yield, c1))
		{
			LOGI("recv %s", r.c_str());
		}
	});
	sch.run_until_complete();

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
			, replace("./", "")
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
	c2.pipeline(ls(), sort(), grep("*fes*"), uniq(), join(), log(), replace("./", ""), out(out_ls));
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
			, log()
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
			, log()
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
			, log()
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
			, log()
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
			, log()
	);
	c1("line1\nline2\nline3\n");
}

TEST(CoroTest, TestCount)
{
	cu::scheduler sch;
	cu::channel<std::string> c1(sch, 100);
	int result;
	c1.pipeline(
			  split("\n")
 			, count()
			, out(result)
	);
	c1("line1\nline2\nline3");
	ASSERT_EQ(result, 3) << "maybe count() is not working well";
}

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
		for(auto& r : cu::range(yield, c1))
		{
			LOGI("recv %s", r.c_str());
			ASSERT_STREQ("HOLA GENTE", r.c_str());
		}
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
				for (auto& s : source)
				{
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
				for (auto& s : source)
				{
					if(s)
					{
						yield((*s) * (*s));
					}
					else
	 					yield(s);
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

//
// TODO: porque channel<T> no es copiable ?
//
// void func1() {}
// void func2() {}
// void foo() {}
// void bar() {}
//
// TEST(CoroTest, Test_Finite_Machine_States)
// {
// 	cu::scheduler sch;
//
// 	cu::channel<float> sensor_cerca(sch, 10);
// 	cu::channel<float> sensor_lejos(sch, 10);
// 	//
// 	cu::channel<bool> on_change_hablarle(sch, 10);
// 	cu::channel<bool> on_change_gritarle(sch, 10);
// 	//
// 	cu::channel<float> update_hablarle(sch, 10);
// 	cu::channel<float> update_gritarle(sch, 10);
//
// 	bool near = true;
//
// 	// selector
// 	sch.spawn([&](auto& yield)
// 	{
// 		for(;;)
// 		{
// 			if(near)
// 				if(near)
// 					func1();
// 				else
// 					foo();
// 			else
// 				bar();
// 			yield();
// 		}
// 	});
//
// 	// sequence
// 	sch.spawn([&](auto& yield)
// 	{
// 		for(;;)
// 		{
// 			func1();
// 			yield();
// 			foo();
// 			yield();
// 			bar();
// 			yield();
// 		}
// 	});
//
//
// 	// selector + sequence
// 	sch.spawn([&](auto& yield)
// 	{
// 		for(;;)
// 		{
// 			if(near)
// 			{
// 				if(near)
// 				{
// 					func1();
// 					yield();
// 					foo();
// 					yield();
// 					bar();
// 					yield();
// 				}
// 				else
// 				{
// 					func1();
// 					yield();
// 					foo();
// 					yield();
// 					bar();
// 					yield();
// 				}
// 			}
// 			else
// 				bar();
// 			yield();
// 		}
// 	});
//
// 	// parallel in subtree
// 	sch.spawn([&](auto& yield)
// 	{
// 		for(;;)
// 		{
// 			if(near)
// 			{
// 				cu::scheduler subsch;
// 				subsch.spawn([&](auto& subyield) {
// 					// thread 1
// 					if(near)
// 					{
// 						if(near)
// 						{
// 							func1();
// 							subyield();
// 							foo();
// 							subyield();
// 							bar();
// 							subyield();
// 						}
// 						else
// 							foo();
// 					}
// 					else if(near)
// 						foo();
// 					else
// 						bar();
// 					subyield();
// 				});
//
// 				subsch.spawn([&](auto& subyield) {
// 					// thread 2
// 					if(near)
// 					{
// 						func1();
// 						subyield();
// 						foo();
// 						subyield();
// 						bar();
// 						subyield();
// 					}
// 					subyield();
// 				});
// 				// subsch.run_until_complete(yield);
// 				subsch.run_until_complete();
// 			}
// 			else
// 			{
// 				bar();
// 				yield();
// 			}
// 		}
// 	});
//
// 	// for
// 	sch.spawn([&](auto& yield)
// 	{
// 		for(;;)
// 		{
// 			for(int i=0;i<10;++i)
// 			{
// 				// go_work(yield);
// 				yield();
// 				// go_eat_something(yield);
// 				yield();
// 				// go_home(yield);
// 				yield();
// 				// go_sleep(yield);
// 				yield();
// 			}
// 			yield();
// 		}
// 	});
//
// 	sch.spawn([&](auto& yield)
// 	{
// 		// perception code (arduino)
// 		// auto x = enemy.get_position();
// 		// bool is_far = x.distance(me) > threshold;
// 		//for(;;)
// 		{
// 			// check sensor
// 			sensor_cerca(yield, 1.0);
// 			sensor_cerca(yield, 1.0);
// 			sensor_cerca(yield, 1.0);
// 			sensor_cerca(yield, 1.0);
// 			sensor_cerca(yield, 1.0);
// 			sensor_lejos(yield, 1.0);
// 			sensor_lejos(yield, 1.0);
// 			sensor_lejos(yield, 1.0);
// 			sensor_lejos(yield, 1.0);
// 			sensor_cerca(yield, 1.0);
// 			sensor_cerca(yield, 1.0);
// 		}
// 	});
// 	sch.spawn([&](auto& yield)
// 	{
// 		// decisions code (raspberry)
// 		// TODO: can generate this code with metaprogramation ?
// 		// proposal:
// 		// cu::fsm(yield, 	std::make_tuple(sensor_cerca, on_change_hablarle, update_hablarle),
// 		// 			std::make_tuple(sensor_lejos, on_change_gritarle, update_gritarle)
// 		//		);
// 		auto tpl = std::make_tuple(std::make_tuple(sensor_cerca, on_change_hablarle, update_hablarle),
// 					   std::make_tuple(sensor_lejos, on_change_gritarle, update_gritarle));
// 		auto& ref_tuple = std::get<0>(tpl);
// 		auto& sensor_prev = std::get<0>(ref_tuple);
// 		auto& change_prev = std::get<1>(ref_tuple);
// 		auto& update_prev = std::get<2>(ref_tuple);
// 		auto& sensor = std::get<0>(ref_tuple);
// 		auto& change = std::get<1>(ref_tuple);
// 		auto& update = std::get<2>(ref_tuple);
// 		int state = -1;
// 		int prev_state = -1;
// 		// TODO:
// 		// while(!sch.forcce_close())
// 		for(;;)
// 		{
// 			prev_state = state;
// 			sensor_prev = sensor;
// 			change_prev = change;
// 			update_prev = update;
// 			{
// 				auto& tuple_channels = std::get<0>(tpl);
// 				sensor = std::get<0>(tuple_channels);
// 				change = std::get<1>(tuple_channels);
// 				update = std::get<2>(tuple_channels);
// 				state = cu::select_nonblock(yield, sensor);
// 				if(state == 0)
// 				{
// 					auto data = sensor.get(yield);
// 					if(data)
// 					{
// 						state = state + 0;
// 						if(prev_state != state)
// 						{
// 							if(prev_state > -1)
// 								change_prev(yield, false);  // prev
// 							change(yield, true);  // state
// 						}
// 						update(yield, *data);
// 						continue;
// 					}
// 				}
// 			}
// 			{
// 				auto& tuple_channels = std::get<1>(tpl);
// 				sensor = std::get<0>(tuple_channels);
// 				change = std::get<1>(tuple_channels);
// 				update = std::get<2>(tuple_channels);
// 				state = cu::select_nonblock(yield, sensor);
// 				if(state == 0)
// 				{
// 					auto data = sensor.get(yield);
// 					if(data)
// 					{
// 						state = state + 1;
// 						if(prev_state != state)
// 						{
// 							if(prev_state > -1)
// 								change_prev(yield, false);  // prev
// 							change(yield, true);  // state
// 						}
// 						update(yield, *data);
// 						continue;
// 					}
// 				}
// 			}
// 			if(state == -1)
// 			{
// 				// nothing changed
// 				state = prev_state;
// 			}
// 		}
// 	});
// 	sch.spawn([&](auto& yield)
// 	{
// 		// action code
// 		//
// 		// while(!sch.forcce_close())
// 		for(;;)
// 		{
// 			#<{(|
// 			// proposal
// 			cu::selector(yield, 
// 				     on_change_hablarle, [](auto& activation){
// 				     	;
// 				     }, 
// 				     update_hablarle, [](auto& value){
// 				     	std::cout << "hablando ..." << std::endl;
// 				     },
// 				     on_change_gritarle, [](auto& activation){
// 				     	;
// 				     }, 
// 				     update_gritarle, [](auto& value){
// 				     	std::cout << "gritando ..." << std::endl;
// 				     });
// 			|)}>#
// 		}
// 	});
// 	// run in slides
// 	sch.run();
// 	sch.run();
// 	sch.run();
// 	sch.run();
// 	sch.run();
// 	sch.run();
// }



TEST(CoroTest, TestMultiConsumer)
{
	cu::scheduler sch;

	cu::channel<int> c1(sch, 10);
	cu::channel<int> c2(sch, 10);
	cu::channel<int> c3(sch, 10);
	
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
		for(int y=51; y<=100; ++y)
		{
			LOGI("2. send %d", y);
			c2(yield, y);
		}
		c2.close(yield);
	});
	sch.spawn([&](auto& yield)
	{
		for(int z=101; z<=150; ++z)
		{
			LOGI("3. send %d", z);
			c3(yield, z);
		}
		c3.close(yield);
	});
	sch.spawn([&](auto& yield)
	{
		int a, b, c;
		for(auto& t : cu::range(yield, c1, c2, c3))
		{
			std::tie(a, b, c) = t;
			LOGI("multiconsume as tuple: a=%d, b=%d, c=%d", a, b, c);
		}
	});
	auto task = asyncply::async(
		[&](){
			sch.run_until_complete();;
		}
	);
	while(!task->is_ready())
	{
		std::cout << "waiting ..." << std::endl;
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	std::cout << "task is complete ..." << std::endl;
}

/*
TEST(CoroTest, TestMQTTCPP)
{
	cu::scheduler sch;
	//
	cu::channel<std::string> habita_light_changed_channel(sch);
	fes::async_fast<fes::marktime, bool> habita_light_changed_event;
	const std::string HABITA_TOPIC_SUB 			{ "/comando/habita/light/changed" };
	const std::string HABITA_TOPIC_PUB 			{ "/comando/habita/light" };
	//
	fes::async_fast<fes::marktime, bool> armario_light_changed_event;
	cu::channel<std::string> armario_light_changed_channel(sch);
	const std::string ARMARIO_TOPIC_SUB 			{ "/comando/armario/light/changed" };
	const std::string ARMARIO_TOPIC_PUB 			{ "/comando/armario/light" };
	//
	const std::string SERVER_ADDRESS	{ "tcp://192.168.1.4:1883" };
	const std::string CLIENT_ID		{ "consumer" };
	const int  QOS = 1;

	mqtt::connect_options connOpts;
	connOpts.set_keep_alive_interval(60);
	connOpts.set_clean_session(false);
	connOpts.set_automatic_reconnect(true);
	mqtt::client cli(SERVER_ADDRESS, CLIENT_ID);

	// CONTROLLERS
	habita_light_changed_event.connect([&](auto marktime, auto state) {
		if(!state)
		{
			auto pubmsg = mqtt::make_message(HABITA_TOPIC_PUB, "true");
			pubmsg->set_qos(QOS);
			cli.publish(pubmsg);
		}
		if(state)
		{
			auto pubmsg = mqtt::make_message(HABITA_TOPIC_PUB, "false");
			pubmsg->set_qos(QOS);
			cli.publish(pubmsg);
		}
	});
	// VIEW
	habita_light_changed_event.connect([](auto marktime, auto state) {
		if(!state)
			std::cout << " <OFF> " << std::endl;
		else
			std::cout << " <ON> " << std::endl;
	});

	try
	{
		cli.connect(connOpts);
		cli.subscribe(HABITA_TOPIC_SUB, QOS);
		std::cout << "OK" << std::endl;

		sch.spawn([&](auto& yield)
		{
			while (true)
			{
				// mqtt -> channel
				auto msg = cli.consume_message();
				if (!msg)
				{
					yield();
				}
				else if(msg->get_topic() == HABITA_TOPIC_SUB)
				{
					habita_light_changed_channel(yield, msg->to_string());
				}
			}
		});
		sch.spawn([&](auto& yield)
		{
			// channel -> event (se le asigna tiempo)
			for(auto& payload : cu::range(yield, habita_light_changed_channel))
			{
				habita_light_changed_event(fes::high_resolution_clock(), payload == "true");
			}
		});
		sch.spawn([&](auto& yield)
		{
			// actualizar eventos
			while(true)
			{
				habita_light_changed_event.update();
				yield();
			}
		});
		sch.run_until_complete();
		cli.disconnect();
	}
	catch (const mqtt::exception& exc)
	{
		std::cerr << exc.what() << std::endl;
	}
}
*/
