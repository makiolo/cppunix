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

TEST(ChannelTest, DISABLED_goroutines_consumer)
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
	for(auto d : go)
	{
		std::cout << "recv: " << d << std::endl;
	}
	task->get();
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
	task->get();
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
