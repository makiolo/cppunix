#include <iostream>
#include <gtest/gtest.h>
#include "../pipeline.h"
#include "../channel.h"
#include "../scheduler.h"
#include "../shell.h"
#include <thread>
#include <asyncply/run.h>

class ChannelTest : testing::Test { };

using cmd = cu::pipeline<int>;

cmd::link generator()
{
	return [](cmd::in&, cmd::out& yield)
	{
		for (auto& s : {100, 200, 300})
		{
			std::cout << "I am generator and push " << s << std::endl;
			yield(s);
		}
	};
}

cmd::link link1()
{
	return [](cmd::in& source, cmd::out& yield)
	{
		for (auto& s : source)
		{
			std::cout << "I am link1 and push " << s << std::endl;
			yield(s);
		}
	};
}

cmd::link link2()
{
	return [](cmd::in& source, cmd::out& yield)
	{
		for (auto& s : source)
		{
			std::cout << "I am link2 and push " << s << std::endl;
			yield(s);
		}
	};
}

cmd::link link3()
{
	return [](cmd::in& source, cmd::out& yield)
	{
		for (auto& s : source)
		{
			std::cout << "I am link3 and push " << s << std::endl;
			yield(s);
		}
	};
}

TEST(ChannelTest, pipeline)
{
	cmd(generator(), link1(), link2(), link3());
}

TEST(ChannelTest, goroutines_consumer)
{
	cu::scheduler sch;
	cu::channel<std::string> go(sch, 8);
	// go.connect(cu::quote("__^-^__"));
	// go.connect(cu::quote("__\o/__"));
	sch.spawn([&](auto& yield) {
		for(;;)
		{
			auto data = go.get(yield);
 			if(data)
 			{
 				std::cout << "recv " << *data << " [PRE] ---->" << std::endl;
				if(go.empty())
					yield();
				std::cout << "recv " << *data << " [POST] ---->" << std::endl;
 			}
 			else
	 		{
			 	std::cout << "channel closed" << std::endl;
 				break;
 			}
		}
	});
	sch.spawn([&](auto& yield) {
		for(int i=0; i<50; ++i)
		{
			std::cout << "----> send " << i << " [PRE]" << std::endl;
			go(yield, std::to_string(i));
			if(go.full())
				yield();
			std::cout << "----> send " << i << " [POST]" << std::endl;
		}
		go.close(yield);
	});
	sch.run_until_complete();
}

TEST(CoroTest, TestScheduler)
{
	cu::scheduler sch;
	cu::semaphore person1(sch);
	cu::semaphore person2(sch);
	cu::semaphore other(sch);
	// person2
	sch.spawn([&](auto& yield) {
		std::cout << "Hola person1" << std::endl;
		person2.notify(yield);
		//
		person1.wait(yield);
		std::cout << "que tal ?" << std::endl;
		person2.notify(yield);
		//
		person1.wait(yield);
		std::cout << "me alegro" << std::endl;
		person2.notify(yield);
		//
		other.notify(yield);
	});
	// person1
	sch.spawn([&](auto& yield) {
		//
		person2.wait(yield);
		std::cout << "Hola person2" << std::endl;
		person1.notify(yield);
		//
		person2.wait(yield);
		std::cout << "bien!" << std::endl;
		person1.notify(yield);
		//
		person2.wait(yield);
		std::cout << "y yo ^^" << std::endl;
		//
		other.notify(yield);
	});
	// other
	sch.spawn([&](auto& yield) {
		//
		other.wait(yield);
		other.wait(yield);
		std::cout << "parar!!! tengo algo importante" << std::endl;
	});
	sch.run_until_complete();
}
