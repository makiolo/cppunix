#include <iostream>
#include <gtest/gtest.h>
#include "../pipeline.h"
#include "../channel.h"
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
	cmd(generator(), link1(), link2(), link3());

	auto handler = [](int s) {
		std::cout << "<fib> received: " << s << " but not modified" << std::endl;
	};

	// channel
	cu::channel<int> go(filter, filter2, handler);
	asyncply::async([&](){
		go << 100;
	});
	asyncply::async([&](){
		go << 200;
	});
	int recv1;
	go >> recv1;
	std::cout << "recv1 is " << recv1 << std::endl;
	int recv2;
	go >> recv2;
	std::cout << "recv2 is " << recv2 << std::endl;
}

