#include <atomic>
#include <string>
#include <unordered_map>
#include <gtest/gtest.h>
#include <teelogging/teelogging.h>
#include <coroutine/coroutine.h>
#include <asyncply/run.h>
#include <asyncply/parallel.h>
#include <mqtt/async_client.h>
#include <fast-event-system/sync.h>
#include <design-patterns-cpp14/memoize.h>
#include <json.hpp>
#include <spdlog/fmt/fmt.h>
#include "../shell.h"
#include "../parallel_scheduler.h"
#include "../semaphore.h"
#include "../channel.h"
#include "../rest.h"


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

/*

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

class sensor : public component
{
public:
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

DEFINE_HASH_CUSTOM(sensor, std::string, "sensor")

namespace
{
	component::memoize::registrator<sensor> reg_sensor;
}


using json = nlohmann::json;

// interruptor/switch/button -> bool (subscribe mandatory and publish optional)
// text -> string (subscribe mandatory and publish optional)
// range/progress -> float (proteger rango con mínimo y máximo) (subscribe mandatory and publish optional)
// list/slider discreto -> int (seleccionar un elemento por posicion) (subscribe mandatory and publish optional)
// color -> rgb (subscribe mandatory and publish optional)
// image -> http://...png (subscribirse y enviar imagenes desde C++ parece más complicado, pero sería necesario para camaras de seguridad)

// subscribe + publisher
// escucha y actua
class interruptor : public sensor
{
public:
	// DEFINE_KEY(interruptor)

	explicit interruptor(cu::parallel_scheduler& parallel_scheduler, mqtt::async_client& client, std::string topic_sub, std::string topic_pub)
		: sensor(parallel_scheduler, client, topic_sub, "")
		, _topic_pub(std::move(topic_pub))
	{
		_client.subscribe(_topic_pub, QOS)->wait();

		if(false)
		{
			// sets desde el ipad
			std::stringstream ss;
			ss << "homebridge/from/set/" << get_name();
			_client.subscribe(ss.str(), QOS)->wait();

			if(endswith(_topic_pub, "/light"))
			{
				std::string name = get_name();
				std::cout << "making interruptor "<<name<<" with sub: " << _topic_sub << " and pub: " << _topic_pub << std::endl;
				// register in homebridge-mqtt
				std::string homebridge_topic = "homebridge/to/add";
				json homebridge_payload = {
					  {"name", name},
					  {"service_name", "light"},
					  {"service", "Switch"}
				};
				auto pubmsg = mqtt::make_message(homebridge_topic, homebridge_payload.dump());
				pubmsg->set_qos(QOS);
				_client.publish(pubmsg)->wait();
			}
		}
	}

	virtual ~interruptor()
	{
		if(false)
		{
			if(endswith(_topic_pub, "/light"))
			{
				std::string name = get_name();

				std::cout << "removing interruptor "<<name<<" with sub: " << _topic_sub << " and pub: " << _topic_pub << std::endl;

				// unregister in homebridge-mqtt
				std::string homebridge_topic = "homebridge/to/remove";
				json homebridge_payload = {
					  {"name", name}
				};
				auto pubmsg = mqtt::make_message(homebridge_topic, homebridge_payload.dump());
				pubmsg->set_qos(QOS);
				_client.publish(pubmsg)->wait();
			}
			// sets desde el ipad
			std::stringstream ss;
			ss << "homebridge/from/set/" << get_name();
			_client.subscribe(ss.str(), QOS)->wait();
		}

		_client.unsubscribe(_topic_pub)->wait();
	}

	std::string get_name() const
	{
		std::string name = "Default";
		if(_topic_pub == "/comando/habita/light")
			name = "habita";
		else if(_topic_pub == "/comando/salon/light")
			name = "salon";
		else if(_topic_pub == "/comando/armario/light")
			name = "armario";
		return name;
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

DEFINE_HASH_CUSTOM(interruptor, std::string, "interruptor")

namespace
{
	component::memoize::registrator<interruptor> reg_interruptor;
}

#include <OIS/OIS.h>
#ifdef _WIN32

#elif defined(__APPLE__)

#elif defined(__linux__)
#include <X11/Xlib.h>
#endif

class InputManager : public OIS::KeyListener, public OIS::MouseListener, public OIS::JoyStickListener
{
public:
	explicit InputManager();
	virtual ~InputManager();
	InputManager(const InputManager&) = delete;
	InputManager& operator=(const InputManager&) = delete;

	void update(cu::yield_type& yield);

	bool keyPressed(const OIS::KeyEvent &arg) override;
	bool keyReleased(const OIS::KeyEvent &arg) override;
	bool mouseMoved(const OIS::MouseEvent &arg) override;
	bool mousePressed(const OIS::MouseEvent &arg, OIS::MouseButtonID id) override;
	bool mouseReleased(const OIS::MouseEvent &arg, OIS::MouseButtonID id) override;
	bool buttonPressed(const OIS::JoyStickEvent &arg, int button) override;
	bool buttonReleased(const OIS::JoyStickEvent &arg, int button) override;
	bool axisMoved(const OIS::JoyStickEvent &arg, int axis) override;
	bool povMoved(const OIS::JoyStickEvent &arg, int pov) override;
	bool vector3Moved(const OIS::JoyStickEvent &arg, int index) override;

	int get_modifier_state();

public:
	fes::sync<OIS::KeyEvent> _keyPressed;
	fes::sync<OIS::KeyEvent> _keyReleased;
	fes::sync<OIS::MouseEvent> _mouseMoved;
	fes::sync<OIS::MouseEvent, OIS::MouseButtonID> _mousePressed;
	fes::sync<OIS::MouseEvent, OIS::MouseButtonID> _mouseReleased;
	fes::sync<OIS::JoyStickEvent, int> _buttonPressed;
	fes::sync<OIS::JoyStickEvent, int> _buttonReleased;
	fes::sync<OIS::JoyStickEvent, int> _axisMoved;
	fes::sync<OIS::JoyStickEvent, int> _povMoved;
	fes::sync<OIS::JoyStickEvent, int> _vector3Moved;
protected:
	OIS::InputManager* _input_manager = 0;
	OIS::Keyboard* _keyboard  = 0;
	OIS::Mouse* _mouse   = 0;
	OIS::JoyStick* _joystick = 0;

	int _width;
	int _height;
};

const char* g_DeviceType[] = {"OISUnknown", "OISKeyboard", "OISMouse", "OISJoyStick", "OISTablet", "OISOther"};

InputManager::InputManager()
	: _keyboard(nullptr)
	, _mouse(nullptr)
	, _joystick(nullptr)
{
	// SDL_GetWindowWMInfo(_window, &system_info);

	std::ostringstream wnd;
#ifdef _WIN32
	// wnd << system_info.info.win.window;
#else
	// wnd << system_info.info.x11.window;
#endif

	OIS::ParamList pl;
	pl.insert(std::make_pair( std::string("WINDOW"), wnd.str() ));
#ifdef _WIN32
	pl.insert(std::make_pair(std::string("w32_mouse"), std::string("DISCL_FOREGROUND" )));
	pl.insert(std::make_pair(std::string("w32_mouse"), std::string("DISCL_NONEXCLUSIVE")));
	pl.insert(std::make_pair(std::string("w32_keyboard"), std::string("DISCL_FOREGROUND")));
	pl.insert(std::make_pair(std::string("w32_keyboard"), std::string("DISCL_NONEXCLUSIVE")))
#else
	pl.insert(std::make_pair(std::string("x11_mouse_grab"), std::string("false")));
	pl.insert(std::make_pair(std::string("x11_mouse_hide"), std::string("false")));
	pl.insert(std::make_pair(std::string("x11_keyboard_grab"), std::string("false")));
	pl.insert(std::make_pair(std::string("XAutoRepeatOn"), std::string("true")));
#endif

	// This never returns null.. it will raise an exception on errors
	_input_manager = OIS::InputManager::createInputSystem(pl);

	// Lets enable all addons that were compiled in:
	_input_manager->enableAddOnFactory(OIS::InputManager::AddOn_All);

	unsigned int v = _input_manager->getVersionNumber();
	std::stringstream ss;
	ss << "OIS Version: " << (v>>16) << "." << ((v>>8) & 0x000000FF) << "." << (v & 0x000000FF)
		<< "\nRelease Name: " << _input_manager->getVersionName()
		<< "\nManager: " << _input_manager->inputSystemName()
		<< "\nTotal Keyboards: " << _input_manager->getNumberOfDevices(OIS::OISKeyboard)
		<< "\nTotal Mice: " << _input_manager->getNumberOfDevices(OIS::OISMouse)
		<< "\nTotal JoySticks: " << _input_manager->getNumberOfDevices(OIS::OISJoyStick);
	LOGI(ss.str().c_str());

	OIS::DeviceList list = _input_manager->listFreeDevices();
	for( OIS::DeviceList::iterator i = list.begin(); i != list.end(); ++i )
	{
		std::cout << "\n\tDevice: " << g_DeviceType[i->first] << " Vendor: " << i->second;
	}
	std::cout << std::endl;

	_keyboard = (OIS::Keyboard*)_input_manager->createInputObject(OIS::OISKeyboard, true);
	_keyboard->setTextTranslation(OIS::Keyboard::Unicode);
	_keyboard->setEventCallback(this);

	// linux SDL2 ya coge el raton de X11
	// http://www.wreckedgames.com/forum/index.php?topic=1233.0
	_mouse = (OIS::Mouse*)_input_manager->createInputObject(OIS::OISMouse, true);
	_mouse->setEventCallback(this);
	const OIS::MouseState &ms = _mouse->getMouseState();
	// ms.width = _width;
	// ms.height = _height;

	try
	{
		// int numSticks = std::min<int>(_input_manager->getNumberOfDevices(OIS::OISJoyStick), 1);
		// if (numSticks > 0)
		// {
			_joystick = (OIS::JoyStick*)_input_manager->createInputObject(OIS::OISJoyStick, true);
			_joystick->setEventCallback(this);
			std::stringstream ss2;
			ss2 << "\n\nCreating Joystick "
				<< "\n\tAxes: " << _joystick->getNumberOfComponents(OIS::OIS_Axis)
				<< "\n\tSliders: " << _joystick->getNumberOfComponents(OIS::OIS_Slider)
				<< "\n\tPOV/HATs: " << _joystick->getNumberOfComponents(OIS::OIS_POV)
				<< "\n\tButtons: " << _joystick->getNumberOfComponents(OIS::OIS_Button)
				<< "\n\tVector3: " << _joystick->getNumberOfComponents(OIS::OIS_Vector3)
				<< std::endl;
			LOGI(ss2.str().c_str());
		// }
	}
	catch(OIS::Exception &ex)
	{
		LOGE("Exception raised on joystick creation: %s", ex.eText);
	}
}

InputManager::~InputManager()
{
	OIS::InputManager::destroyInputSystem(_input_manager);
}

void InputManager::update(cu::yield_type& yield)
{
	_keyboard->capture();
	_mouse->capture();
	if( _joystick )
	{
		_joystick->capture();
	}
	yield( cu::control_type{} );
}

bool InputManager::keyPressed(const OIS::KeyEvent &arg)
{
	_keyPressed(arg);
	return true;
}

bool InputManager::keyReleased(const OIS::KeyEvent &arg)
{
	_keyReleased(arg);
	return true;
}

bool InputManager::mouseMoved(const OIS::MouseEvent &arg)
{
	_mouseMoved(arg);
	return true;
}

bool InputManager::mousePressed(const OIS::MouseEvent &arg, OIS::MouseButtonID id)
{
	_mousePressed(arg, id);
	return true;
}

bool InputManager::mouseReleased(const OIS::MouseEvent &arg, OIS::MouseButtonID id)
{
	_mouseReleased(arg, id);
	return true;
}

bool InputManager::buttonReleased(const OIS::JoyStickEvent &arg, int button)
{
	_buttonReleased(arg, button);
	return true;
}

bool InputManager::buttonPressed(const OIS::JoyStickEvent &arg, int button)
{
	_buttonPressed(arg, button);
	return true;
}

bool InputManager::axisMoved(const OIS::JoyStickEvent &arg, int axis)
{
	_axisMoved(arg, axis);
	return true;
}

bool InputManager::povMoved(const OIS::JoyStickEvent &arg, int pov)
{
	_povMoved(arg, pov);
	return true;
}

bool InputManager::vector3Moved(const OIS::JoyStickEvent &arg, int index)
{
	_vector3Moved(arg, index);
	return true;
}

int InputManager::get_modifier_state()
{
	int modifier_state = 0;

	// if (_keyboard->isModifierDown(OIS::Keyboard::Ctrl))
	// 	modifier_state |= Rocket::Core::Input::KM_CTRL;
	// if (_keyboard->isModifierDown(OIS::Keyboard::Shift))
	// 	modifier_state |= Rocket::Core::Input::KM_SHIFT;
	// if (_keyboard->isModifierDown(OIS::Keyboard::Alt))
	// 	modifier_state |= Rocket::Core::Input::KM_ALT;

#ifdef _WIN32

	// if (GetKeyState(VK_CAPITAL) > 0)
	// 	modifier_state |= Rocket::Core::Input::KM_CAPSLOCK;
	// if (GetKeyState(VK_NUMLOCK) > 0)
	// 	modifier_state |= Rocket::Core::Input::KM_NUMLOCK;
	// if (GetKeyState(VK_SCROLL) > 0)
	// 	modifier_state |= Rocket::Core::Input::KM_SCROLLLOCK;

#elif defined(__APPLE__)

	// UInt32 key_modifiers = GetCurrentEventKeyModifiers();
	// if (key_modifiers & (1 << alphaLockBit))
	// 	modifier_state |= Rocket::Core::Input::KM_CAPSLOCK;

#elif defined(__linux__)

	// TODO:
	// XKeyboardState keyboard_state;
	// XGetKeyboardControl(DISPLAY!, &keyboard_state);
	//
	// if (keyboard_state.led_mask & (1 << 0))
	//	modifier_state |= Rocket::Core::Input::KM_CAPSLOCK;
	// if (keyboard_state.led_mask & (1 << 1))
	//	modifier_state |= Rocket::Core::Input::KM_NUMLOCK;
	// if (keyboard_state.led_mask & (1 << 2))
	//	modifier_state |= Rocket::Core::Input::KM_SCROLLLOCK;

#endif

	return modifier_state;
}

bool startswith(const std::string& text,const std::string& token)
{
	if(text.length() < token.length())
		return false;
	return (text.compare(0, token.length(), token) == 0);
}

auto interruptor_homie_from_topic_subscribe(cu::parallel_scheduler& sch, mqtt::async_client& cli, const std::string& topic_)
{
	std::stringstream ss;
	ss << topic_ << "/set";
	std::string publisher = ss.str();
	std::string subscribe = topic_;
	return component::memoize::instance().get<interruptor>(sch, cli, subscribe, publisher);
}

auto interruptor_homie_from_topic_publisher(cu::parallel_scheduler& sch, mqtt::async_client& cli, const std::string& topic_)
{
	std::string publisher = topic_;
	std::string subscribe = dirname(publisher);
	return component::memoize::instance().get<interruptor>(sch, cli, subscribe, publisher);
}

auto interruptor_homie_from_name(cu::parallel_scheduler& sch, mqtt::async_client& cli, const std::string& room)
{
	std::stringstream ss;
	ss << "homie/" << room << "/button/on";
	std::string subscribe = ss.str();
	return interruptor_homie_from_topic_subscribe(sch, cli, subscribe);
}

auto interruptor_from_topic_subscribe(cu::parallel_scheduler& sch, mqtt::async_client& cli, const std::string& topic_)
{
	std::string short_topic = dirname(topic_);
	return component::memoize::instance().get<interruptor>(sch, cli, topic_, short_topic);
}

auto interruptor_from_topic_publisher(cu::parallel_scheduler& sch, mqtt::async_client& cli, const std::string& topic_)
{
	std::string short_topic = topic_;
	std::string topic = topic_ + "/changed";
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

static std::map<std::string, std::vector<std::tuple<fes::marktime, std::string> > > marktimes;
static std::map<std::string, std::map<std::string, bool> > presences;

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

TEST(CoroTest, DISABLED_TestMQTTCPP)
{
	// InputManager input;

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
			// sch.spawn([&](auto& yield)
			// {
			// 	input.update(yield);
			// });
			sch.spawn([&](auto& yield)
			{
				cu::await(yield, cli.subscribe("/comando/+/light", QOS) );
				cu::await(yield, cli.subscribe("/comando/+/light/changed", QOS) );
				cu::await(yield, cli.subscribe("homie/+/+/presence", QOS) );
				cu::await(yield, cli.subscribe("homie/+/button/on", QOS) );
			});
			sch.spawn([&](auto& yield)
			{
				std::vector<std::shared_ptr<component> > cached;
				while (true)
				{
					auto msg = asyncply::await(yield, [&](){ return cli.consume_message(); });
					if(!msg)
					{
						yield( cu::control_type{} );
					}
					else
					{
						std::string topic = msg->get_topic();
						std::string value = msg->to_string();
						std::cout << "topic = " << topic << std::endl;
						std::cout << "payload = " << value << std::endl;

						if(startswith(topic, "homebridge/from/set"))
						{
							// un cambio de estado que viene del ipad
							json j = json::parse(value);
							auto interrup = interruptor_from_name(sch, cli, j["name"].get<std::string>());
							if(j["value"].get<bool>())
							{
								std::cout << "sending true" << std::endl;
								interrup->channel()(yield, "true");
							}
							else
							{
								std::cout << "sending false" << std::endl;
								interrup->channel()(yield, "false");
							}
						}
						else
						{
							if(value == "true" || value == "false")
							{
								if(endswith(msg->get_topic(), "/light/changed"))
								{
									std::string value = msg->to_string();
									auto interrup = interruptor_from_topic_subscribe(sch, cli, msg->get_topic());
									interrup->channel()(yield, value);
									cached.push_back(interrup);
								}

								else if(endswith(msg->get_topic(), "/light"))
								{
									std::string value = msg->to_string();
									auto interrup = interruptor_from_topic_publisher(sch, cli, msg->get_topic());
									interrup->on_change()(fes::high_resolution_clock(), interrup->payload_to_state(value));
									cached.push_back(interrup);
								}
								else if(endswith(msg->get_topic(), "/presence"))
								{
									std::string value = msg->to_string();
									auto sensor = component::memoize::instance().get("sensor", sch, cli, msg->get_topic(), "");
									sensor->on_change()(fes::high_resolution_clock(), sensor->payload_to_state(value));
									cached.push_back(sensor);
								}
							}
						}
					}
				}
			});

			auto habita = interruptor_from_name(sch, cli, "habita");
			auto armario = interruptor_from_name(sch, cli, "armario");
			auto salon = interruptor_from_name(sch, cli, "salon");

			auto mesilla = interruptor_homie_from_name(sch, cli, "mesilla");

			auto salon_presence_1 = sensor_from_name(sch, cli, "salon", "presence_1");
			auto salon_presence_2 = sensor_from_name(sch, cli, "salon", "presence_2");
			auto salon_presence_3 = sensor_from_name(sch, cli, "salon", "presence_3");

			auto habita_presence_1 = sensor_from_name(sch, cli, "habita", "presence_1");
			auto habita_presence_2 = sensor_from_name(sch, cli, "habita", "presence_2");
			auto habita_presence_3 = sensor_from_name(sch, cli, "habita", "presence_3");

			sch.spawn([&](auto& yield)
			{
				while(true)
				{
					mesilla->on();
					cu::sleep(yield, fes::deltatime(1000));
					mesilla->off();
					cu::sleep(yield, fes::deltatime(1000));
				}
			});

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
				// if(salon->is_on())
				{
					salon->on();
				}
				else
				{
					salon->off();
				}
				//////////////////////////////
				if(has_presence("habita"))
				// if(habita->is_on())
				{
					habita->on();
				}
				else
				{
					habita->off();
				}
				//////////////////////////////
				if(has_presence("armario"))
				// if(armario->is_on())
				{
					armario->on();
				}
				else
				{
					armario->off();
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
			// habita->on_change().connect([](auto marktime, auto state) {
			// 	if(!state)
			// 		std::cout << " <habita OFF> " << std::endl;
			// 	else
			// 		std::cout << " <habita ON> " << std::endl;
			// });
			// armario->on_change().connect([](auto marktime, auto state) {
			// 	if(!state)
			// 		std::cout << " <armario OFF> " << std::endl;
			// 	else
			// 		std::cout << " <armario ON> " << std::endl;
			// });
			// salon->on_change().connect([](auto marktime, auto state) {
			// 	if(!state)
			// 		std::cout << " <salon OFF> " << std::endl;
			// 	else
			// 		std::cout << " <salon ON> " << std::endl;
			// });

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

*/

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

TEST(CoroTest, DISABLED_Rest1)
{
	using namespace fmt::literals;

	// while(true)
	{
		cu::parallel_scheduler sch;
		cu::channel<json> read(sch);
		read.pipeline( cu::get() );
		sch.spawn([&](auto& yield) {
			read(yield, json{ {"url", "https://api.coinmarketcap.com/v1/ticker/?limit=0&convert=EUR"} });
			read.close(yield);
		});
		sch.spawn([&](auto& yield) {
			auto to_string = [](auto elem) -> std::string {
				if(!elem.is_null())
				{
					// escape space, comma or equal
					std::string data = elem.template get<std::string>();
					data = cu::replace_all(data, " ", "\\ ");
					data = cu::replace_all(data, ",", "\\,");
					data = cu::replace_all(data, "=", "\\=");
					return data;
				}
				else
					return "";
			};
			auto to_float = [](auto elem) -> float {
				if(!elem.is_null())
					return std::stof(elem.template get<std::string>());
				else
					return 0.0f;
			};
			auto to_integer = [](auto elem) -> int {
				if(!elem.is_null())
					return std::stoi(elem.template get<std::string>());
				else
					return 0;
			};
			std::string hostname = "localhost";
			int port = 8086;
			std::string database = "domotica";
			std::string measurement = "coinmarketcap2";
			for(auto& jsn : cu::range(yield, read))
			{
				auto node = jsn["last_updated"];
				if(!node.is_null())
				{
					auto id = to_string(jsn["id"]);
					auto name = to_string(jsn["name"]);
					auto symbol = to_string(jsn["symbol"]);
					auto rank = to_integer(jsn["rank"]);
					auto price_eur = to_float(jsn["price_eur"]);
					auto last_updated = to_integer(jsn["last_updated"]);
					std::cout << "---------------------" << std::endl;
					std::cout << jsn << std::endl;
					std::cout << "---------------------" << std::endl;
					cu::curl_post(
						"http://{}:{}/write?db={}"_format(hostname, port, database), 
						"{},id={},name={},symbol={} rank={},price_eur={} {}"_format(
							measurement, 
							id, name, symbol, 
							rank, price_eur,
							last_updated));
				}
			}
		});
		sch.run_until_complete();
	}
}

