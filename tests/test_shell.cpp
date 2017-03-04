#include <atomic>
#include "../shell.h"
#include "../coroutine.h"
#include "../scheduler.h"
#include "../semaphore.h"
#include <teelogging/teelogging.h>
#include <gtest/gtest.h>

class CoroTest : testing::Test { };

using namespace cu;

TEST(CoroTest, Test_run_ls_strip_quote_grep)
{
	cmd(
			  run("ls .")
			, strip()
			, quote()
			, grep("shell_*")
			, assert_count(1)
			, assert_string("\"shell_exe\"")
	);

	cmd(
			  ls(".")
			, strip()
			, quote()
			, grep("shell_*")
			, assert_count(1)
			, assert_string("\"shell_exe\"")
	);
}

TEST(CoroTest, Test_run_ls_sort_grep_uniq_join)
{
	std::string out_subproces;
	cmd(run("ls ."), strip(), sort(), grep("lib*"), uniq(), join(), out(out_subproces));
	//
	std::string out_ls;
	cmd(ls("."), sort(), grep("lib*"), uniq(), join(), out(out_ls));
	//
	ASSERT_STREQ(out_subproces.c_str(), out_ls.c_str());
}

TEST(CoroTest, TestCut)
{
	cmd(
			  in("hello big world")
			, assert_count(1)
			, split()
			, assert_count(3)
			, join()
			, assert_count(1)
			, cut(0)
			, assert_count(1)
			, assert_string("hello")
	);
	cmd(
			  in("hello big world")
			, assert_count(1)
			, split()
			, assert_count(3)
			, join()
			, assert_count(1)
			, cut(1)
			, assert_count(1)
			, assert_string("big")
	);
	cmd(
			  in("hello big world")
			, assert_count(1)
			, split()
			, assert_count(3)
			, join()
			, assert_count(1)
			, cut(2)
			, assert_count(1)
			, assert_string("world")
	);
}

TEST(CoroTest, TestGrep)
{
	cmd(	  
			  in("line1\nline2\nline3")
			, split("\n")
			, assert_count(3)
			, grep("line2")
			, assert_count(1)
			, assert_string("line2")
	);
}

TEST(CoroTest, TestGrep2)
{
	cmd(	  
			  in("line1\nline2\nline3\n")
			, split("\n")
			, assert_count(4)
	);
}

TEST(CoroTest, TestScheduler)
{
	const int N = 16;
	cu::scheduler sch;
	cu::semaphore maria(sch);
	cu::semaphore ricardo(sch);
	cu::semaphore other(sch);
	// Ricardo
	sch.spawn("ricardo", [&](auto& yield) {
		std::cout << "Hola Maria" << std::endl;
		ricardo.notify(yield);
		//
		maria.wait(yield);
		std::cout << "que tal ?" << std::endl;
		ricardo.notify(yield);
		//
		maria.wait(yield);
		std::cout << "me alegro nena" << std::endl;
		ricardo.notify(yield);
		//
		other.notify(yield);
	});
	// Maria
	sch.spawn("maria", [&](auto& yield) {
		//
		ricardo.wait(yield);
		std::cout << "Hola Ricardo" << std::endl;
		maria.notify(yield);
		//
		ricardo.wait(yield);
		std::cout << "bien!" << std::endl;
		maria.notify(yield);
		//
		ricardo.wait(yield);
		std::cout << "y yo ^^" << std::endl;
		//
		other.notify(yield);
	});
	// other
	sch.spawn("other", [&](auto& yield) {
		//
		other.wait(yield);
		other.wait(yield);
		std::cout << "parar!!! tengo algo importante" << std::endl;
	});
	sch.run_until_complete();
}

