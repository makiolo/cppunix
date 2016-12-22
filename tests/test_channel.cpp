#include <iostream>
#include <gtest/gtest.h>
#include "../pipeline.h"
#include "../channel.h"
#include <thread>

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
	std::cout << "I am filter and push " << s*2 << std::endl;
	return s*2;
}

void filter2(int s)
{
	std::cout << "I am filter received " << s << " but not modified" << std::endl;
}

TEST(ChannelTest, goroutines_or_something_like_that)
{
	// pipeline
	// cmd(generator(), link1(), link2(), link3());

	auto handler = [](int s) {
		std::cout << "<fib> received: " << s << " but not modified" << std::endl;
	};

	// channel
	cu::channel<int> go(filter, filter2, handler);
	std::thread t1([&](){
		go << 100;
	});
	std::thread t2([&](){
		go << 200;
	});
	std::cout << "recv1 is " << go.get() << std::endl;
	std::cout << "recv2 is " << go.get() << std::endl;
	t1.join();
	t2.join();
}

