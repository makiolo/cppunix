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

cmd::link receiver(cu::push_type<int>& r)
{
	return [&](cmd::in& source, cmd::out& yield)
	{
		for (auto& s : source)
		{
			// send to receiver
			r(s);
			// continue chaining
			yield(s);
		}
	};
}

TEST(ChannelTest, goroutines_or_something_like_that)
{
	// pipeline
	cmd(generator(), link1(), link2(), link3());

	// lambda to register
	auto l1 = [](int s) {
		std::cout << "<fib> received: " << s << std::endl;
	};
	auto r = cu::push_type<int>(
		[&](cu::pull_type<int>& source) {
			for (auto& s : source)
			{
				l1(s);
			}
		}
	);

	// channel
	cu::channel<int> go;
	go.connect(link1());
	go.connect(link2());
	go.connect(link3());
	go.connect(receiver(r));
	go(1);
	go(3);
	go(5);
	go(7);
	go(9);
}

