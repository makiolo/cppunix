#ifndef _CU_SHELL_H_
#define _CU_SHELL_H_

#include <set>
#include <string>
#include <fstream>
#include <iostream>
#include <sstream>
#include <exception>
#include <vector>

#include <boost/tokenizer.hpp>
#include <boost/filesystem.hpp>
#include <boost/regex.hpp>
#include <boost/algorithm/string.hpp>

////
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>

#ifdef LINUX
#include <sys/types.h>
#include <sys/stat.h>
#endif

#ifdef WIN32

#include <io.h>

#ifndef DEVNUL
#define DEVNUL "NUL"
#endif

#ifndef dup
#define dup _dup
#endif

#ifndef dup2
#define dup2 _dup2
#endif

#else

#ifndef DEVNUL
#define DEVNUL "/dev/null"
#endif

// pipe() and close()
#include <unistd.h>

#endif
////

#include "pipeline.h"

namespace cu {

using cmd = cu::pipeline<std::string>;
	
cmd::link cat(const std::string& filename)
{
	return [&](cmd::in&, cmd::out& yield)
	{
		std::ifstream input(filename);
		for (std::string line; std::getline(input, line);)
		{
			yield(line);
		}
	};
}

cmd::link cat()
{
	return [&](cmd::in& source, cmd::out& yield)
	{
		for (auto s : source)
		{
			cat(s)(source, yield);
		}
	};
}

void find_tree(const boost::filesystem::path& p, cmd::out& yield)
{
	namespace fs = boost::filesystem;
	if(fs::is_directory(p))
	{
		for (auto f = fs::directory_iterator(p); f != fs::directory_iterator(); ++f)
		{
			if(fs::is_directory(f->path()))
			{
				find_tree(f->path(), yield);
			}
			else
			{
				yield(f->path().string());
			}
		}
	}
	else
	{
		yield(p.string());
	}
}

cmd::link find(const std::string& dir)
{
	return [&](cmd::in&, cmd::out& yield)
	{
		boost::filesystem::path p(dir);
		if (boost::filesystem::exists(p))
		{
			find_tree(p, yield);
		}
	};
}
	
cmd::link find()
{
	return [&](cmd::in& source, cmd::out& yield)
	{
		for (auto s : source)
		{
			find(s)(source, yield);
		}
	};
}

cmd::link ls(const std::string& dir)
{
	namespace fs = boost::filesystem;
	return [&](cmd::in&, cmd::out& yield)
	{
		fs::path p(dir);
		if (fs::exists(p) && fs::is_directory(p))
		{
			for (auto f = fs::directory_iterator(p); f != fs::directory_iterator(); ++f)
			{
				if (fs::is_regular_file(f->path()))
				{
					yield(f->path().string());
				}
			}
		}
	};
}
	
cmd::link ls()
{
	return [&](cmd::in& source, cmd::out& yield)
	{
		for (auto s : source)
		{
			ls(s)(source, yield);
		}
	};
}

cmd::link grep(const std::string& pattern, bool exclusion = false)
{
	return [&](cmd::in& source, cmd::out& yield)
	{
		const boost::regex re(pattern);
		for (auto s : source)
		{
			const std::string& line(s);
			boost::match_results<std::string::const_iterator> groups;
			if ((boost::regex_search(line, groups, re) && (groups.size() > 0)) == !exclusion)
			{
				yield(line);
			}
		}
	};
}

cmd::link grep_v(const std::string& pattern)
{
	return [&](cmd::in& source, cmd::out& yield)
	{
		grep(pattern, true)(source, yield);
	};
}

cmd::link contain(const std::string& in)
{
	return [&](cmd::in& source, cmd::out& yield)
	{
		for (auto s : source)
		{
			const std::string& line(s);
			if (line.find(in) != std::string::npos)
			{
				yield(line);
			}
		}
	};
}

cmd::link uniq()
{
	return [&](cmd::in& source, cmd::out& yield)
	{
		std::set<std::string> unique;
		for (auto s : source)
		{
			unique.insert(s);
		}
		for (const auto& s : unique)
		{
			yield(s);
		}
	};
}

cmd::link ltrim()
{
	return [&](cmd::in& source, cmd::out& yield)
	{
		for (auto s : source)
		{
			std::string buf(s);
			buf.erase(buf.begin(), std::find_if(buf.begin(), buf.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
			yield(buf);
		}
	};
}

cmd::link rtrim()
{
	return [&](cmd::in& source, cmd::out& yield)
	{
		for (auto s : source)
		{
			std::string buf(s);
			buf.erase(std::find_if(buf.rbegin(), buf.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), buf.end());
			yield(buf);
		}
	};
}

cmd::link trim()
{
	return [&](cmd::in& source, cmd::out& yield)
	{
		ltrim()(source, yield);
		rtrim()(source, yield);
	};
}

cmd::link cut(int field, const char* delim = " ")
{
	return [&](cmd::in& source, cmd::out& yield)
	{
		typedef boost::tokenizer<boost::char_separator<char>> tokenizer;
		for (auto s : source)
		{
			int i = 0;
			for (auto& t : tokenizer(s, boost::char_separator<char>(delim)))
			{
				if (i++ == field)
				{
					yield(t);
					break;
				}
			}
		}
	};
}

cmd::link quote(const char* delim = "\"")
{
	return [&](cmd::in& source, cmd::out& yield)
	{
		for (auto s : source)
		{
			std::stringstream ss;
			ss << delim << s << delim;
			yield(ss.str());
		}
	};
}

cmd::link join(const char* delim = " ")
{
	return [&](cmd::in& source, cmd::out& yield)
	{
		std::stringstream ss;
		int i = 0;
		for (auto s : source)
		{
			if(i != 0)
				ss << delim << s;
			else
				ss << s;
			++i;
		}
		yield(ss.str());
	};
}

cmd::link split(const std::string& text, const char* delim = " ", bool keep_empty=true)
{
	return [&](cmd::in&, cmd::out& yield)
	{
		std::vector<std::string> chunks;
		boost::split(chunks, text, boost::is_any_of(delim));
		for(auto& chunk : chunks)
		{
			if(!keep_empty && chunk.empty())
				continue;
			yield(chunk);
		}
	};
}
	
cmd::link split(const char* delim = " ", bool keep_empty=true)
{
	return [&](cmd::in& source, cmd::out& yield)
	{
		for (auto s : source)
		{
			split(s, delim, keep_empty)(source, yield);
		}
	};
}

cmd::link assert_string(const std::string& matching)
{
	return [&](cmd::in& source, cmd::out& yield)
	{
		for (auto s : source)
		{
			if(matching != s)
			{
				std::stringstream ss;
				ss << "error in string: " << s << ", expected value: " << matching << std::endl;
				throw std::runtime_error(ss.str());
			}
			yield(s);
		}
	};
}

cmd::link assert_string(const std::vector<std::string>& matches)
{
	return [&](cmd::in& source, cmd::out& yield)
	{
		int i = 0;
		for (auto s : source)
		{
			if(matches[i] != s)
			{
				std::stringstream ss;
				ss << "error in string: " << s << ", expected value: " << matches[i] << std::endl;
				throw std::runtime_error(ss.str());
			}
			++i;
			yield(s);
		}
	};
}
	
class file_redirect
{
public:
	explicit file_redirect(FILE* original = stdout, FILE* destiny = nullptr);
	~file_redirect();

private:
	FILE* _original;
	FILE* _destiny;
	bool _to_null;
	int _backup;
};

file_redirect::file_redirect(FILE* original, FILE* destiny)
	: _original(original)
	, _destiny(destiny)
	, _to_null(destiny == nullptr)
{
	if(_to_null)
	{
		_destiny = fopen(DEVNUL, "a");
	}

	// backup original pipes
	_backup = dup(fileno(_original));
	if (_backup == -1)
	{
		std::abort();
	}

	fflush(_original);
	
	// overwrite original with destiny (dup2 is not thread safe)
	int res = dup2(fileno(_destiny), fileno(_original));
	if (res == -1)
	{
		std::abort();
	}
}

file_redirect::~file_redirect()
{
	fflush(_original);

	// recover original (dup2 is not thread safe)
	int res = dup2(_backup, fileno(_original));
	if (res == -1)
	{
		std::abort();
	}

	// closes
	close(_backup);
	if(_to_null)
	{
		fclose(_destiny);
	}
}


cmd::link in()
{
	return [&](cmd::in&, cmd::out& yield)
	{
		for (std::string line; std::getline(std::cin, line);)
		{
			yield(line);
		}
	};
}
	
cmd::link in(const std::vector<std::string>& strs)
{
	return [&](cmd::in&, cmd::out& yield)
	{
		for(auto& str : strs)
		{
			yield(str);
		}
	};
}

cmd::link in(const std::string& str)
{
	return [&](cmd::in&, cmd::out& yield)
	{
		yield(str);
	};
}

cmd::link out(std::vector<std::string>& strs)
{
	return [&](cmd::in& source, cmd::out& yield)
	{
		for (auto s : source)
		{
			strs.emplace_back(s);
			yield(s);
		}
	};
}

cmd::link out()
{
	return [&](cmd::in& source, cmd::out& yield)
	{
		for (auto s : source)
		{
			std::cout << s << std::endl;
			yield(s);
		}
	};
}

cmd::link run(const std::string& cmd)
{
	char buff[BUFSIZ];
	return [cmd, &buff](cmd::in&, cmd::out& yield)
	{
		file_redirect silence_err(stderr, stdout);
		
		FILE *in;
		if(!(in = popen(cmd.c_str(), "r")))
		{
			std::stringstream ss;
			ss << "Error executing command: " << cmd;
			throw std::runtime_error(ss.str());
		}
		while(fgets(buff, BUFSIZ, in) != 0)
		{
			split(std::string(buff), "\n")(source, yield);
			/*
			// remove endline
			std::string newline(buff);
			newline.erase(std::remove(newline.begin(), newline.end(), '\n'), newline.end());
			yield(newline);
			*/
		}
		pclose(in);
	};
}

}

#endif
