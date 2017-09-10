#include <iostream>
#include <gtest/gtest.h>
#include <teelogging/teelogging.h>
#include "../channel.h"

class PipelineTest : testing::Test { };

TEST(PipelineTest, Test_fibonacci_n4134)
{
	/*
	// n4134: http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2014/n4134.pdf
	generator<int> fib(int n)
	{
		int a = 0;
		int b = 1;
		while (n-- > 0)
		{
			yield a;
			auto next = a + b;
			a = b;
			b = next;
		}
	}
	*/

	auto fib = [](int n_) {
		return cu::pull_type<int>(
			[&](cu::push_type<int>& yield) {
				int n = n_;
				int a = 0;
				int b = 1;
				while (n-- > 0)
				{
					yield (a);
					auto next = a + b;
					a = b;
					b = next;
				}
			}
		);
	};
	for (auto& v : fib(233))
	{
		std::cout << v << std::endl;
		if (v > 10)
			break;
	}
}

TEST(PipelineTest, Test_recursive_n4134)
{
	std::function<cu::pull_type<int>(int,int)> range_rec = [&range_rec](int a_, int b_) -> cu::pull_type<int> {
		return cu::pull_type<int>(
			[&](auto& yield) {
				int a = a_;
				int b = b_;
				/////////////////////
				auto n = b - a;
				if (n <= 0)
					return;
				if (n == 1)
				{
					yield (a);
					return;
				}

				auto mid = a + n / 2;

				// original proposal is:
				//     yield range(a, mid)
				//     yield range(mid, b)

				for (auto i : range_rec(a, mid))
					yield (i);
				for (auto i : range_rec(mid, b))
					yield (i);
				///////////////////////
			}
		);
	};

	LOGV("range recursive");
	for (auto v : range_rec(1, 10))
		LOGV("%d", v);
}

TEST(PipelineTest, Test3)
{
	auto fib = [](int n) {
		return cu::push_type<int>(
			[&](cu::pull_type<int>& source) {
				for (auto& s : source)
				{
					LOGV("<fib(%d)> received: %d", n, s);
				}
			}
		);
	};
	auto fib20 = fib(20);
	fib20(1);
	fib20(3);
	fib20(7);
}

