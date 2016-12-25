#include <iostream>
#include <gtest/gtest.h>
#include "../pipeline.h"
#include "../channel.h"
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
	// channel
	cu::channel<int> go(100);
	
	/*
	auto handler = [](auto data) {
		return data*2;
	};
	go.connect(handler);
	*/

	auto task = asyncply::async([&](){
		for(int i=0; i<100; ++i)
		{
			go << i;
		}
		go.close();
	});
	for(auto d : go)
	{
		std::cout << "recv: " << d << std::endl;
	}
	task.get();
}

TEST(ChannelTest, goroutines_consumer2)
{
	// channel
	cu::channel<int> go(100);
	auto task = asyncply::async([&](){
		for(int i=0; i<100; ++i)
		{
			go << i;
		}
		go.close();
	});
	for(;;)
	{
		auto data = go.get();
		if(!data)
		{
			std::cout << "channel closed" << std::endl;
			break;
		}
		else
		{
			std::cout << "recv: " << *data << std::endl;
		}
	}
	task.get();
}

TEST(ChannelTest, goroutines_consumer3)
{
	cu::channel<int> go;
	std::cout << "produce in channel" << std::endl;
	go << 100;
	auto data = go.get();
	if(data)
	{
		std::cout << "data = " << *data << std::endl;
	}
	else
	{
		std::cout << "channel closed" << std::endl;
	}
}

/*
TEST(ChannelTest, scheduler_basic)
{
 	std::vector<cu::pull_type_ptr<int> > coros;
 	for(int i=1; i<10; ++i)
 	{
 		coros.emplace_back(cu::make_generator<int>(
 			[=](auto& yield)
 			{
 				std::cout << "create " << i << std::endl;
 				yield(0);
 				std::cout << "download " << i << std::endl;
 				yield(1);
 				std::cout << "patching " << i << std::endl;
 				yield(2);
 				std::cout << "compile " << i << std::endl;
 				yield(3);
 				std::cout << "tests " << i << std::endl;
 				yield(4);
 				std::cout << "packing " << i << std::endl;
 				yield(5);
 				std::cout << "destroy " << i << std::endl;
 			}
 		));
 	}

 	bool any_updated = true;
 	while(any_updated)
 	{
 		any_updated = false;
 		for(auto& c : coros)
 		{
 			if(*c)
 			{
 				int ret = (*c)();
				std::cout << "ret = " << ret << std::endl;
 				any_updated = true;
 			}
 		}
 	}
}
*/
