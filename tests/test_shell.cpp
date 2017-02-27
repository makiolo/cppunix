#include <atomic>
#include "../shell.h"
#include "../coroutine.h"
#include <gtest/gtest.h>

class CoroTest : testing::Test { };

using namespace cu;

TEST(CoroTest, Test1)
{
	std::vector<std::string> lines;
	cmd(find("../.."), grep("test_"), out(lines));
	for (auto& line : lines)
		std::cout << line << std::endl;
}

TEST(CoroTest, Test2)
{
	cmd(find("../.."),
		grep(".*\\.cpp$|.*\\.h$"),
		cat(),
		grep("class|struct|typedef|using|void|int|double|float"),
		grep_v("enable_if|;|\"|\'"),
		split(" "),
		trim(),
		uniq(),
		join(" "),
		out());
}

namespace cu {

class scheduler {
public:
	using control_type = void;
	
	template <typename Function>
	void spawn(Function&& func)
	{
		_running.emplace_back(cu::make_generator<control_type>(std::forward<Function>(func)));
	}
		
	/*
	return true if any is updated
	*/
	bool run()
	{
		bool any_updated = false;
		_pid = 0;
		for(auto& c : _running)
		{
			if(*c)
			{
				(*c)();
				any_updated = true;
			}
			++_pid;
		}
		return any_updated;
	}
	
	/*
	return true if have pending work
	*/
	// run_forever()
	// run_until_complete()
	void run_until_complete()
	{
		bool any_updated = true;
		while(any_updated)
		{
			any_updated = run();
		}
	}
	
	void run_forever()
	{
		while(true)
		{
			run();
		}
	}
	
	int getpid() const {return _pid;}
	
protected:
	// normal running
	std::vector<cu::pull_type_ptr<control_type> > _running;
	// locked
	// std::vector<cu::pull_type_ptr<control_type> > _blocked;
	// unlocked and prepared for return
	// std::vector<cu::pull_type_ptr<control_type> > _ready;
private:
	int _pid;
};
	
}

TEST(CoroTest, Test3)
{
	cu::scheduler sch;
	for(int i=1; i<10; ++i)
	{
		sch.spawn(
			[=](auto& yield) {
				std::cout << "create " << i << std::endl;
				yield();
				std::cout << "download " << i << std::endl;
				yield();
				std::cout << "patching " << i << std::endl;
				yield();
				std::cout << "compile " << i << std::endl;
				yield();
				std::cout << "tests " << i << std::endl;
				yield();
				std::cout << "packing " << i << std::endl;
				yield();
				std::cout << "destroy " << i << std::endl;
			}
		);
	}
	sch.run_until_complete();
}
