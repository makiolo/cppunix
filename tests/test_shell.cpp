#include <atomic>
#include <string>
#include <gtest/gtest.h>
#include <teelogging/teelogging.h>
#include "../shell.h"
#include "../coroutine.h"
#include "../scheduler.h"
#include "../semaphore.h"
#include "../channel.h"

class CoroTest : testing::Test { };

using namespace cu;


TEST(CoroTest, Test_run_ls_strip_quote_grep)
{
	cu::scheduler sch;

	cu::channel<std::string> c1(sch, 100);
	c1.pipeline(
			  run()
			, strip()
			, quote()
			, grep("shell_*")
			, assert_count(1)
			, assert_string("\"shell_exe\"")
			, out()
	);
	c1("ls .");

	/////////////////////////////////

	cu::channel<std::string> c2(sch, 100);
	c2.pipeline(
			  ls()
			, strip()
			, quote()
			, grep("shell_*")
			, assert_count(1)
			, assert_string("\"shell_exe\"")
			, out()
	);
	c2(".");
}

TEST(CoroTest, Test_run_ls_sort_grep_uniq_join)
{
	cu::scheduler sch;

	cu::channel<std::string> c1(sch, 100);
	std::string out_subproces;
	c1.pipeline(run(), strip(), sort(), grep("*fes*"), uniq(), join(), out(), out(out_subproces));
	c1("ls .");
	//
	cu::channel<std::string> c2(sch, 100);
	std::string out_ls;
	c2.pipeline(ls(), sort(), grep("*fes*"), uniq(), join(), out(), out(out_ls));
	c2(".");
	//
	ASSERT_STREQ(out_subproces.c_str(), out_ls.c_str());
}

TEST(CoroTest, TestCut)
{
	cu::scheduler sch;

	cu::channel<std::string> c1(sch, 100);
	c1.pipeline(
			  assert_count(1)
			, split()
			, assert_count(3)
			, join()
			, assert_count(1)
			, cut(0)
			, assert_count(1)
			, assert_string("hello")
	);
	c1("hello big world");

	cu::channel<std::string> c2(sch, 100);
	c2.pipeline(
			  assert_count(1)
			, split()
			, assert_count(3)
			, join()
			, assert_count(1)
			, cut(1)
			, assert_count(1)
			, assert_string("big")
	);
	c2("hello big world");

	cu::channel<std::string> c3(sch, 100);
	c3.pipeline(
			  assert_count(1)
			, split()
			, assert_count(3)
			, join()
			, assert_count(1)
			, cut(2)
			, assert_count(1)
			, assert_string("world")
	);
	c3("hello big world");
}

TEST(CoroTest, TestGrep)
{
	cu::scheduler sch;

	cu::channel<std::string> c1(sch, 100);
	c1.pipeline(
			  split("\n")
			, assert_count(3)
			, grep("line2")
			, assert_count(1)
			, assert_string("line2")
	);
	c1("line1\nline2\nline3");
}

TEST(CoroTest, TestGrep2)
{
	cu::scheduler sch;

	cu::channel<std::string> c1(sch, 100);
	c1.pipeline(
			  split("\n")
			, assert_count(4)
	);
	c1("line1\nline2\nline3\n");
}


TEST(CoroTest, TestCount)
{
	// cu::channel<std::string> c1(sch, 100);
	// int result;
	// c1.pipeline(
	// 		  split("\n")
	// 		, count()
	// 		, out(result)
	// );
	// c1("line1\nline2\nline3");
	// ASSERT_EQ(result, 3) << "maybe count() is not working well";
}

TEST(CoroTest, TestUpper)
{
	// cu::scheduler sch_test;
	// cu::channel<std::string> c1(sch_test, 100);
	// c1.pipeline(toupper(), out());
	// sch.spawn([&](auto& yield){
	// 	c1(yield, "hola mundo");
	// });
	// sch_test.run_until_complete();
	// std::cout << c1.get() << std::endl;
}

TEST(CoroTest, TestScheduler2)
{
	cu::scheduler sch;

	cu::channel<int> c1(sch, 5);
	cu::channel<int> c2(sch, 5);
	cu::channel<int> c3(sch, 5);

	sch.spawn([&](auto& yield) {
		LOGI("start productor 1 / 2");
		for(int x=0; x<100; ++x)
		{
			LOGI("tick productor 1 / 2: sending: %d", x);
			c1(yield, x);
		}
		LOGI("closing productor 1 / 2");
		c1.close(yield);
		LOGI("closed productor 1 / 2");
	});
	sch.spawn([&](auto& yield) {
		LOGI("start productor 2 / 2");
		for(int z=0; z<100; ++z)
		{
			LOGI("tick productor 2 / 2: sending: %d", z);
			c2(yield, z);
		}
		LOGI("closing productor 2 / 2");
		c2.close(yield);
		LOGI("closed productor 2 / 2");
	});
	sch.spawn([&](auto& yield) {
		LOGI("start consume a - b");
		for(;;)
		{
			LOGI("get consume a");
			auto a = c1.get(yield);
 			if(a)
			{
				LOGI("get consume b");
				auto b = c2.get(yield);
				if(b)
				{
					c3(yield, *a + *b);
				}
				else
				{
					break;
				}
			}
 			else
			{
 				break;
			}
		}
		c3.close(yield);
	});
	sch.spawn([&](auto& yield) {
		LOGI("start consume final");
		c3.for_each(yield, [](auto& r) {
			LOGI("result = %d", r);
		});

	});
	sch.run_until_complete();
}

