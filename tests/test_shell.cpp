#include <atomic>
#include <string>
#include <unordered_map>
#include <gtest/gtest.h>
#include <teelogging/teelogging.h>
#include <coroutine/coroutine.h>
#include "../shell.h"
#include "../parallel_scheduler.h"
#include "../semaphore.h"
#include "../channel.h"
#include <asyncply/run.h>
#include <asyncply/parallel.h>
#include <mqtt/async_client.h>
#include <fast-event-system/sync.h>
#include <fast-event-system/sync.h>
#include <design-patterns-cpp14/memoize.h>

class CoroTest : testing::Test { };

using namespace cu;


TEST(CoroTest, Test_find)
{
	cu::parallel_scheduler sch;
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
	cu::parallel_scheduler sch;

	cu::channel<std::string> c1(sch, 100);
	c1.pipeline(
			  run()
			, strip()
			, quote()
			, grep("shell_*")
			, assert_count(1)
			, assert_string("\"shell_unittest\"")
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
			, assert_string("\"shell_unittest\"")
			, log()
	);
	c2(".");
}

TEST(CoroTest, Test_run_ls_sort_grep_uniq_join)
{
	cu::parallel_scheduler sch;

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
	cu::parallel_scheduler sch;

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
	cu::parallel_scheduler sch;

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
	cu::parallel_scheduler sch;

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
	cu::parallel_scheduler sch;
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
	cu::parallel_scheduler sch;
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
	cu::parallel_scheduler sch;

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
// 	cu::parallel_scheduler sch;
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
// 				cu::parallel_scheduler subsch;
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
	cu::parallel_scheduler sch;

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

///////////////////////////////////////////////////////////////// MQTT C++ ////////////////////////////////////////////////////////////////////////////////////////////

namespace std {
	template <>
	struct hash<cu::parallel_scheduler&>
	{
		size_t operator()(cu::parallel_scheduler&) const
		{
			return std::hash<std::string>()("parallel_scheduler");
		}
	};
}

namespace std {
	template <>
	struct hash<mqtt::async_client&>
	{
		size_t operator()(mqtt::async_client&) const
		{
			return std::hash<std::string>()("mqtt_client");
		}
	};
}

namespace std {
	template <>
	struct hash<const std::string&>
	{
		size_t operator()(const std::string& value) const
		{
			return std::hash<std::string>()(value);
		}
	};
}

const int  QOS = 1;
const auto TIMEOUT = std::chrono::seconds(10);

class component
{
public:
	using memoize = dp14::memoize<component, cu::parallel_scheduler&, mqtt::async_client&, const std::string&, const std::string&>;
	virtual ~component() { ; }

	virtual fes::sync<fes::marktime, bool>& on_change() = 0;
	virtual const fes::sync<fes::marktime, bool>& on_change() const = 0;

	virtual cu::channel<std::string>& channel() = 0;
	virtual const cu::channel<std::string>& channel() const = 0;

	virtual bool payload_to_state(const std::string& value) const = 0;
	virtual std::string state_to_payload(bool value) const = 0;
};

// template <typename T>
class sensor : public component
{
public:
	// DEFINE_KEY(sensor<T>)
	
	explicit sensor(cu::parallel_scheduler& parallel_scheduler, mqtt::async_client& client, std::string topic_sub, std::string topic_pub_unsed)
		: _scheduler(parallel_scheduler)
		, _client(client)
		, _topic_sub(std::move(topic_sub))
		, _on_value("true")
		, _off_value("false")
		, _channel(parallel_scheduler)
		, _state(false)
	{
		this->on_change()(fes::high_resolution_clock(), _state);
		_client.subscribe(_topic_sub, QOS)->wait();
		_scheduler.spawn([&](auto& yield)
		{
			for(auto& payload : cu::range(yield, this->channel()))
			{
				bool new_state = this->payload_to_state(payload);
				if(new_state != this->_state)
				{
					this->_state = new_state;
					this->on_change()(fes::high_resolution_clock(), new_state);
				}
			}
		});
	}

	virtual ~sensor()
	{
		_client.unsubscribe(_topic_sub)->wait();
	}

	bool payload_to_state(const std::string& value) const override final
	{
		return value == _on_value;
	}

	std::string state_to_payload(bool value) const override final
	{
		if(value)
			return _on_value;
		else
			return _off_value;
	}

	bool is_on() const
	{
		return _state;
	}

	fes::sync<fes::marktime, bool>& on_change() override final
	{
		return _event;
	}

	const fes::sync<fes::marktime, bool>& on_change() const override final
	{
		return _event;
	}

	cu::channel<std::string>& channel() override final
	{
		return _channel;
	}

	const cu::channel<std::string>& channel() const override final
	{
		return _channel;
	}

protected:
	cu::parallel_scheduler& _scheduler;
	mqtt::async_client& _client;
	std::string _topic_sub;
	std::string _on_value;
	std::string _off_value;
	cu::channel<std::string> _channel;
	fes::sync<fes::marktime, bool> _event;
	bool _state;
};
DEFINE_HASH(sensor)
// DEFINE_HASH(sensor<bool>)
// DEFINE_HASH(sensor<float>)

namespace
{
	component::memoize::registrator<sensor> reg_sensor;
	// component::memoize::registrator<sensor<bool> > reg_sensor_bool;
	// component::memoize::registrator<sensor<float> > reg_sensor_float;
}

/*
interruptor/switch/button -> bool (subscribe mandatory and publish optional)
text -> string (subscribe mandatory and publish optional)
range/progress -> float (proteger rango con mínimo y máximo) (subscribe mandatory and publish optional)
list/slider discreto -> int (seleccionar un elemento por posicion) (subscribe mandatory and publish optional)
color -> rgb (subscribe mandatory and publish optional)
image -> http://...png (subscribirse y enviar imagenes desde C++ parece más complicado, pero sería necesario para camaras de seguridad)
*/

// subscribe + publisher
// escucha y actua
class interruptor : public sensor
{
public:
	DEFINE_KEY(interruptor)

	explicit interruptor(cu::parallel_scheduler& parallel_scheduler, mqtt::async_client& client, std::string topic_sub, std::string topic_pub)
		: sensor(parallel_scheduler, client, topic_sub, "")
		, _topic_pub(std::move(topic_pub))
	{
		_client.subscribe(_topic_pub, QOS)->wait();
	}

	virtual ~interruptor()
	{
		_client.unsubscribe(_topic_pub)->wait();
	}

	mqtt::delivery_token_ptr on() const
	{
		auto pubmsg = mqtt::make_message(_topic_pub, state_to_payload(true));
		pubmsg->set_qos(QOS);
		return _client.publish(pubmsg);
	}

	mqtt::delivery_token_ptr off() const
	{
		auto pubmsg = mqtt::make_message(_topic_pub, state_to_payload(false));
		pubmsg->set_qos(QOS);
		return _client.publish(pubmsg);
	}

	mqtt::delivery_token_ptr toggle() const
	{
		if(is_on())
			return off();
		else
			return on();
	}

protected:
	std::string _topic_pub;
};

namespace
{
	component::memoize::registrator<interruptor> reg_interruptor;
}

std::string dirname(const std::string& str)
{
	std::size_t found = str.find_last_of("/");
	return str.substr(0, found);
}

bool endswith(const std::string& text, const std::string& ending)
{
	if (text.length() >= ending.length())
	{
		return (0 == text.compare (text.length() - ending.length(), ending.length(), ending));
	}
	else
	{
		return false;
	}
}

bool startswith(const std::string& text,const std::string& token)
{
	if(text.length() < token.length())
		return false;
	return (text.compare(0, token.length(), token) == 0);
}

auto interruptor_from_topic_subscribe(cu::parallel_scheduler& sch, mqtt::async_client& cli, const std::string& topic_)
{
	std::string short_topic = dirname(topic_);
	return component::memoize::instance().get<interruptor>(sch, cli, topic_, short_topic);
}

auto interruptor_from_topic_publisher(cu::parallel_scheduler& sch, mqtt::async_client& cli, const std::string& topic_)
{
	std::string topic = topic_ + "/changed";
	std::string short_topic = topic;
	return component::memoize::instance().get<interruptor>(sch, cli, topic, short_topic);
}

auto interruptor_from_name(cu::parallel_scheduler& sch, mqtt::async_client& cli, const std::string& room)
{
	std::stringstream ss;
	ss << "/comando/" << room << "/light/changed";
	std::string topic = ss.str();
	std::string short_topic = dirname(topic);
	return component::memoize::instance().get<interruptor>(sch, cli, topic, short_topic);
}

auto sensor_from_name(cu::parallel_scheduler& sch, mqtt::async_client& cli, const std::string& room, const std::string& sensor, const std::string& kind = "presence")
{
	std::stringstream ss;
	ss << "homie/" << room << "/" << sensor << "/" << kind;
	std::string topic = ss.str();
	return component::memoize::instance().get("sensor", sch, cli, topic, "");
}

std::map<std::string, std::vector<std::tuple<fes::marktime, std::string> > > marktimes;
std::map<std::string, std::map<std::string, bool> > presences;

fes::deltatime get_shutdown_time(std::string location)
{
	// return fes::deltatime(35000);
	return fes::deltatime(10000);
}

bool _has_presence(std::string location, std::vector<std::string> sensors)
{
	auto marktime = fes::high_resolution_clock() - get_shutdown_time(location);
	for(auto& tpl : marktimes[location])
	{
		fes::marktime timestamp;
		std::string sensor;
		std::tie(timestamp, sensor) = tpl;
		if(std::find(sensors.begin(), sensors.end(), sensor) != sensors.end())
		{
			if(timestamp >= marktime)
			{
				return true;
			}
			else
			{
				break;
			}
		}
	}
	return false;
}

bool has_presence(std::string location)
{
	if(location == "armario")
		return _has_presence("habita", {"presence_3"});
	else
		return _has_presence(location, {"presence_1", "presence_2", "presence_3"});
}

TEST(CoroTest, TestMQTTCPP)
{
	presences["salon"]["presence_1"] = false;
	presences["salon"]["presence_2"] = false;
	presences["salon"]["presence_3"] = false;
	presences["habita"]["presence_1"] = false;
	presences["habita"]["presence_2"] = false;
	presences["habita"]["presence_3"] = false;

	mqtt::async_client cli("tcp://192.168.1.4:1883", "cppclient");
	try
	{
		mqtt::connect_options connOpts;
		connOpts.set_keep_alive_interval(60);
		connOpts.set_clean_session(false);
		connOpts.set_automatic_reconnect(true);
		cli.connect(connOpts)->wait();
		cli.start_consuming();
		{
			cu::parallel_scheduler sch;
			sch.spawn([&](auto& yield)
			{
				cu::await(yield, cli.subscribe("/comando/+/light", QOS) );
				cu::await(yield, cli.subscribe("/comando/+/light/changed", QOS) );
				cu::await(yield, cli.subscribe("homie/+/+/presence", QOS) );
			});
			sch.spawn([&](auto& yield)
			{
				while (true)
				{
					auto msg = asyncply::await(yield, [&](){ return cli.consume_message(); });
					if(!msg)
					{
						yield( cu::control_type{} );
					}
					else
					{
						std::string value = msg->to_string();
						if(value == "true" || value == "false")
						{
							if(endswith(msg->get_topic(), "/light/changed"))
							{
								std::string value = msg->to_string();
								auto interrup = interruptor_from_topic_subscribe(sch, cli, msg->get_topic());
								interrup->channel()(yield, value);
							}
							else if(endswith(msg->get_topic(), "/light"))
							{
								std::string value = msg->to_string();
								auto interrup = interruptor_from_topic_publisher(sch, cli, msg->get_topic());
								interrup->on_change()(fes::high_resolution_clock(), interrup->payload_to_state(value));
							}
							else if(endswith(msg->get_topic(), "/presence"))
							{
								std::string value = msg->to_string();
								auto sensor = component::memoize::instance().get("sensor", sch, cli, msg->get_topic(), "");
								sensor->on_change()(fes::high_resolution_clock(), sensor->payload_to_state(value));
							}
						}
					}
				}
			});

			auto habita = interruptor_from_name(sch, cli, "habita");
			auto armario = interruptor_from_name(sch, cli, "armario");
			auto salon = interruptor_from_name(sch, cli, "salon");

			auto salon_presence_1 = sensor_from_name(sch, cli, "salon", "presence_1");
			auto salon_presence_2 = sensor_from_name(sch, cli, "salon", "presence_2");
			auto salon_presence_3 = sensor_from_name(sch, cli, "salon", "presence_3");

			auto habita_presence_1 = sensor_from_name(sch, cli, "habita", "presence_1");
			auto habita_presence_2 = sensor_from_name(sch, cli, "habita", "presence_2");
			auto habita_presence_3 = sensor_from_name(sch, cli, "habita", "presence_3");

			// auto salon_lux = sensor_from_name(sch, cli, "salon", "lux", "lux");
			// auto habita_lux = sensor_from_name(sch, cli, "habita", "lux", "lux");
			// salon_lux->on_change().connect([&](auto marktime, auto state){
			// 	;
			// });

			auto controller = [&](auto marktime, auto state, auto location, auto node_id)
			{
				// presences
				presences[location][node_id] = state;

				// marktimes
				bool location_presence_1 = presences[location]["presence_1"];
				bool location_presence_2 = presences[location]["presence_2"];
				bool location_presence_3 = presences[location]["presence_3"];
				if(location_presence_1 || location_presence_2 || location_presence_3)
				{
					if(location_presence_3)
					{
						marktimes[location].push_back(std::make_tuple(marktime, "presence_3"));
					}
					else
					{
						marktimes[location].push_back(std::make_tuple(marktime, "presence_1"));
					}
				}

				//////////////////////////////
				if(has_presence("salon"))
				{
					// std::cout << "salon on" << std::endl;
					salon->on()->wait();
				}
				else
				{
					// std::cout << "salon off" << std::endl;
					salon->off()->wait();
				}
				//////////////////////////////
				if(has_presence("habita"))
				{
					// std::cout << "habita on" << std::endl;
					habita->on()->wait();
				}
				else
				{
					// std::cout << "habita off" << std::endl;
					habita->off()->wait();
				}
				//////////////////////////////
				if(has_presence("armario"))
				{
					// std::cout << "armario on" << std::endl;
					armario->on()->wait();
				}
				else
				{
					// std::cout << "armario off" << std::endl;
					armario->off()->wait();
				}
				//////////////////////////////
			};

			// salon
			salon_presence_1->on_change().connect(std::bind(controller, std::placeholders::_1, std::placeholders::_2, "salon", "presence_1"));
			salon_presence_2->on_change().connect(std::bind(controller, std::placeholders::_1, std::placeholders::_2, "salon", "presence_2"));
			salon_presence_3->on_change().connect(std::bind(controller, std::placeholders::_1, std::placeholders::_2, "salon", "presence_3"));

			// habita
			habita_presence_1->on_change().connect(std::bind(controller, std::placeholders::_1, std::placeholders::_2, "habita", "presence_1"));
			habita_presence_2->on_change().connect(std::bind(controller, std::placeholders::_1, std::placeholders::_2, "habita", "presence_2"));
			habita_presence_3->on_change().connect(std::bind(controller, std::placeholders::_1, std::placeholders::_2, "habita", "presence_3"));

			// view
			habita->on_change().connect([](auto marktime, auto state) {
				if(!state)
					std::cout << " <habita OFF> " << std::endl;
				else
					std::cout << " <habita ON> " << std::endl;
			});
			armario->on_change().connect([](auto marktime, auto state) {
				if(!state)
					std::cout << " <armario OFF> " << std::endl;
				else
					std::cout << " <armario ON> " << std::endl;
			});
			salon->on_change().connect([](auto marktime, auto state) {
				if(!state)
					std::cout << " <salon OFF> " << std::endl;
				else
					std::cout << " <salon ON> " << std::endl;
			});
			sch.run_until_complete();
		}
		cli.stop_consuming();
		cli.disconnect()->wait();
	}
	catch (const mqtt::exception& exc)
	{
		std::cerr << exc.what() << std::endl;
	}
}

TEST(CoroTest, Asyncply1)
{
	cu::parallel_scheduler sch;
	sch.spawn([&](auto& yield)
	{
		int n = asyncply::await(yield, asyncply::aparallel(
			[]()
			{
				return 3;
			}, 

			[]()
			{
				return 8;
			}, 

			[]()
			{
				return 10;
			})
		);
		EXPECT_EQ(n, 21);
	});
	sch.run_until_complete();
}


TEST(CoroTest, Asyncply2)
{
	cu::parallel_scheduler sch;
	sch.spawn([&](auto& yield)
	{
		int n = asyncply::await(yield, asyncply::aparallel(
			[]()
			{
				return 4;
			}, 

			[]()
			{
				return 10;
			})
		);
		EXPECT_EQ(n, 14);
	});
	sch.spawn([&](auto& yield)
	{
		int n = asyncply::await(yield, asyncply::aparallel(
			[]()
			{
				return 56;
			}, 

			[]()
			{
				return 10;
			})
		);
		EXPECT_EQ(n, 66);
	});
	sch.run_until_complete();
}

TEST(CoroTest, Test4)
{
	auto coro = cu::pull_type<int>(
		[&](cu::push_type<int>& yield) {
			yield(5);
			yield(6);
			yield(7);
		}
	);

	while(coro)
	{
		auto n = coro.get();
		LOGI("----- push element = %d", n);
		coro();
	}
}

