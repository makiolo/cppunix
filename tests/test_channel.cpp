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

void filter1(int s)
{
	std::cout << "1. I am filter received " << s << " but not modified" << std::endl;
}

int filter2(int s)
{
	std::cout << "2. I am filter and push " << s*2 << std::endl;
	return s*2;
}

TEST(ChannelTest, goroutines_or_something_like_that)
{
	// pipeline
	// cmd(generator(), link1(), link2(), link3());

	auto handler = [](int s) {
		std::cout << "3. received: " << s << " but not modified" << std::endl;
	};

	// channel
	cu::channel<int> go(filter2, handler);
	go.connect(filter1);
	std::thread t1([&](){
		go << 100;
	});
	std::thread t2([&](){
		go << 200;
	});
	std::cout << "4. recv1 is " << go.get() << std::endl;
	std::cout << "4. recv2 is " << go.get() << std::endl;
	t1.join();
	t2.join();
}
