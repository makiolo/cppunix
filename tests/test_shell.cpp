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
	cmd(run("ls ."), strip(), sort(), grep("*fes*"), uniq(), join(), out(), out(out_subproces));
	//
	std::string out_ls;
	cmd(ls("."), sort(), grep("*fes*"), uniq(), join(), out(), out(out_ls));
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


TEST(CoroTest, TestCount)
{
	int result;
	cmd(	  
			  in("line1\nline2\nline3")
			, split("\n")
			, count()
			, out(result)
	);
	ASSERT_EQ(result, 3) << "maybe count() is not working well";
}
