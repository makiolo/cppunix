#include <iostream>
#include <gtest/gtest.h>
#include "../pipeline.h"
#include "../channel.h"
#include "../scheduler.h"
#include <thread>
#include <asyncply/run.h>

// TODO: remove
#include <set>
#include <string>
#include <fstream>
#include <iostream>
#include <sstream>
#include <exception>
#include <vector>
#include <algorithm>
#include <locale>
#include <boost/tokenizer.hpp>
#include <boost/filesystem.hpp>
#include <boost/regex.hpp>
#include <boost/algorithm/string.hpp>

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
	cu::channel<std::string> go(sch, 7);
	// go.connect(cu::quote("__^-^__"));
	// go.connect(cu::quote("__\o/__"));
	
	// https://play.golang.org/
	/*
package main

import "fmt"

func worker(done chan int) {
    for i := 0; i < 50; i++ {
        fmt.Println("----> send ", i, " [PRE]")
	done <- i
        fmt.Println("----> send ", i, " [POST]")
    }
}

func main() {
    done := make(chan int, 8)
    go worker(done)
    for i := 0; i < 50; i++ {
	j := <- done
	fmt.Println("recv ", j, "  <---- ")
    }
}
	*/
	
	sch.spawn([&](auto& yield) {
		for(;;)
		{
			auto data = go.get(yield);
 			if(data)
 			{
 				std::cout << "recv " << *data << " <----" << std::endl;
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
			std::cout << "----> send " << i << " [POST]" << std::endl;
		}
		go.close(yield);
	});
	sch.run_until_complete();
}

cu::channel<std::string>::link quot(const char* delim = "\"")
{
	return [=](cu::channel<std::string>::in& source, cu::channel<std::string>::out& yield)
	{
		for (auto& s : source)
		{
			if(s)
			{
				std::stringstream ss;
				ss << delim << *s << delim;
				yield(ss.str());
			}
			else
			{
				// propagate error
				yield(s);
			}
		}
	};
}

TEST(ChannelTest, goroutines_consumer_unbuffered)
{
	cu::scheduler sch;
	cu::channel<std::string> go(sch, 0, quot("1__^-^__1"), quot("2__\o/__2"));
	sch.spawn([&](auto& yield) {
		for(;;)
		{
			auto data = go.get(yield);
 			if(data)
 			{
 				std::cout << "recv " << *data << " <----" << std::endl;
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
			std::cout << "----> send " << i << " [POST]" << std::endl;
		}
		go.close(yield);
	});
	sch.run_until_complete();
}

TEST(ChannelTest, goroutines_consumer_buffered_one)
{
	cu::scheduler sch;
	cu::channel<std::string> go(sch, 1);
	// go.connect(cu::quote("__^-^__"));
	// go.connect(cu::quote("__\o/__"));	
	sch.spawn([&](auto& yield) {
		for(;;)
		{
			auto data = go.get(yield);
 			if(data)
 			{
 				std::cout << "recv " << *data << " <----" << std::endl;
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
			std::cout << "----> send " << i << " [POST]" << std::endl;
		}
		go.close(yield);
	});
	sch.run_until_complete();
}

TEST(ChannelTest, goroutines_consumer_buffered_two)
{
	cu::scheduler sch;
	cu::channel<std::string> go(sch, 2);
	// go.connect(cu::quote("__^-^__"));
	// go.connect(cu::quote("__\o/__"));	
	sch.spawn([&](auto& yield) {
		for(;;)
		{
			auto data = go.get(yield);
 			if(data)
 			{
 				std::cout << "recv " << *data << " <----" << std::endl;
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
