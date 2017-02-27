#include <atomic>
#include "../shell.h"
#include "../coroutine.h"
#include <teelogging/teelogging.h>
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

TEST(CoroTest, TestRun)
{
	cmd(run("ls"), out());
	std::cout << "---------" << std::endl;
	cmd(run("ls dasjkdaskjd.txt"), out());
}

namespace cu {

class scheduler {
public:
	using control_type = void;
	
	template <typename Function>
	void spawn(Function&& func)
	{
		_running.emplace_back(cu::make_generator<control_type>(
			[&func](auto& yield) {
				yield();
				(std::forward<Function>(func))(yield);
			}
		));
	}
		
	/*
	return true if any is updated
	*/
	bool run()
	{
		bool any_updated = false;
		_pid = 0;
		LOGD("total = %d", _running.size());
		for(auto& c : _running)
		{
			LOGD("ticking = %d", getpid());
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
	
	// auto create_semaphore();
	
	// each semaphore have 2 channels and ret code
	
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
	const int N = 16;
	cu::scheduler sch;
	for(int i=0; i<N; ++i)
	{
		sch.spawn([&](auto& yield) {
			std::cout << "create " << i << " - pid: " << sch.getpid() << std::endl;
			yield();
			std::cout << "download " << i << " - pid: " << sch.getpid() << std::endl;
			yield();
			std::cout << "patching " << i << " - pid: " << sch.getpid() << std::endl;
			yield();
			std::cout << "compile " << i << " - pid: " << sch.getpid() << std::endl;
			yield();
			std::cout << "tests " << i << " - pid: " << sch.getpid() << std::endl;
			yield();
			std::cout << "packing " << i << " - pid: " << sch.getpid() << std::endl;
			yield();
			std::cout << "destroy " << i << " - pid: " << sch.getpid() << std::endl;
		});
	}
	sch.run_until_complete();
}
