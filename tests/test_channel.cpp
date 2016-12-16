#include <iostream>
#include <gtest/gtest.h>
#include "../pipeline.h"
#include "../channel.h"

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

int filter(int s)
{
	std::cout << "I am filter and push " << s << std::endl;
	return s*2;
}

TEST(ChannelTest, goroutines_or_something_like_that)
{
	// pipeline
	cmd(generator(), link1(), link2(), link3());
	
	auto handler = [](int s) {
		std::cout << "<fib> received: " << s << std::endl;
		return s;
	};

	// channel
	cu::channel<int> go;
	go.connect(filter);
	go.connect(handler);
	go(1);
	go(3);
	go(5);
	go(7);
	go(9);
}
