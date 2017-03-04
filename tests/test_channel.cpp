#include <iostream>
#include <gtest/gtest.h>
#include "../pipeline.h"
#include "../channel.h"
#include "../scheduler.h"
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
	cu::channel<int> go(sch, 80);
	sch.spawn("producer", [&sch, &go](auto& yield) {

		// channel
		for(int i=0; i<100; ++i)
		{
			std::cout << "sending: " << i << std::endl;
			go(yield, i);
		}
		go.close(yield);
	});
	sch.spawn("consumer", [&sch, &go](auto& yield) {

		go.sync(yield);

		// for(auto data : go)
		// {
		// 	std::cout << "recving: " << data << std::endl;
		// }

		for(;;)
		{
			auto data = go.get(yield);
			if(!data)
			{
				std::cout << "channel closed" << std::endl;
				break;
			}
			else
			{
				std::cout << "recving: " << *data << std::endl;
			}
		}
	});
	sch.run_until_complete();
}

