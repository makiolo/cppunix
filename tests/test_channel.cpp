#include <iostream>
#include <gtest/gtest.h>
#include "../channel.h"
#include "../parallel_scheduler.h"
#include "../shell.h"
#include <thread>
#include <asyncply/run.h>

class ChannelTest : testing::Test { };

TEST(ChannelTest, goroutines_consumer)
{
	cu::parallel_scheduler sch;
	cu::channel<std::string> go(sch);
	go.pipeline(cu::quote("<html>"), cu::quote("<head>"));
	
	sch.spawn([&](auto& yield) {
		for(auto& data : cu::range(yield, go))
		{
			std::cout << "recv " << data << " <----" << std::endl;
		}
	});
	sch.spawn([&](auto& yield) {
		for(int i=0; i<50; ++i) {
			std::cout << "----> send " << i << " [PRE]" << std::endl;
			go(yield, std::to_string(i));
			std::cout << "----> send " << i << " [POST]" << std::endl;
		}
		go.close(yield);
	});
	sch.run_until_complete();
}

TEST(ChannelTest, goroutines_consumer_unbuffered)
{
	cu::parallel_scheduler sch;
	cu::channel<std::string> go(sch);
	go.pipeline(cu::quote("<html>"), cu::quote("<head>"));
	sch.spawn([&](auto& yield) {
		for(auto& data : cu::range(yield, go))
		{
			std::cout << "recv " << data << " <----" << std::endl;
		}
	});
	sch.spawn([&](auto& yield) {
		for(int i=0; i<50; ++i) {
			std::cout << "----> send " << i << " [PRE]" << std::endl;
			go(yield, std::to_string(i));
			std::cout << "----> send " << i << " [POST]" << std::endl;
		}
		go.close(yield);
	});
	sch.run_until_complete();
}

TEST(CoroTest, TestScheduler)
{
	cu::parallel_scheduler sch;
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

