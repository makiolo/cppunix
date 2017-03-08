#ifndef _CU_SHELL_H_
#define _CU_SHELL_H_

#include <set>
#include <string>
#include <fstream>
#include <iostream>
#include <sstream>
#include <exception>
#include <vector>
#include <algorithm>
#include <locale>

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

std::string replace_all(const std::string &str, const char *from, const char *to)
{
    std::string result(str);
    std::string::size_type
        index = 0,
        from_len = strlen(from),
        to_len = strlen(to);
    while ((index = result.find(from, index)) != std::string::npos) {
        result.replace(index, from_len, to);
        index += to_len;
    }
    return result;
}
	
std::string translate(const char *pattern)
{
    // from https://gist.github.com/alco/1869512
    int i = 0, n = strlen(pattern);
    std::string result;

    while (i < n) {
        char c = pattern[i];
        ++i;

        if (c == '*') {
            result += ".*";
        } else if (c == '?') {
            result += '.';
        } else if (c == '[') {
            int j = i;
            /*
             * The following two statements check if the sequence we stumbled
             * upon is '[]' or '[!]' because those are not valid character
             * classes.
             */
            if (j < n && pattern[j] == '!')
                ++j;
            if (j < n && pattern[j] == ']')
                ++j;
            /*
             * Look for the closing ']' right off the bat. If one is not found,
             * escape the opening '[' and continue.  If it is found, process
             * the contents of '[...]'.
             */
            while (j < n && pattern[j] != ']')
                ++j;
            if (j >= n) {
                result += "\\[";
            } else {
                std::string stuff = replace_all(std::string(&pattern[i], j - i), "\\", "\\\\");
                char first_char = pattern[i];
                i = j + 1;
                result += "[";
                if (first_char == '!') {
                    result += "^" + stuff.substr(1);
                } else if (first_char == '^') {
                    result += "\\" + stuff;
                } else {
                    result += stuff;
                }
                result += "]";
            }
        } else {
            if (isalnum(c)) {
                result += c;
            } else {
                result += "\\";
                result += c;
            }
        }
    }
    /*
     * Make the expression multi-line and make the dot match any character at all.
     */
    return result + "\\Z(?ms)";
}
	
cmd::link cat(const std::string& filename)
{
	return [=](cmd::in&, cmd::out& yield)
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
	return [=](cmd::in&, cmd::out& yield)
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
	return [=](cmd::in&, cmd::out& yield)
	{	
		fs::path full_path(dir);

		if ( fs::exists( full_path ) )
		{
			if ( fs::is_directory( full_path ) )
			{
				fs::directory_iterator end_iter;
				for ( fs::directory_iterator dir_itr( full_path ); dir_itr != end_iter; ++dir_itr )
				{
					yield( dir_itr->path().string() );
				}
			}
			else
			{
				yield( full_path.string() );
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

cmd::link grep(const char* pattern, bool exclusion = false)
{
	return [=](cmd::in& source, cmd::out& yield)
	{
		const boost::regex re(translate(pattern));
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

cmd::link grep_v(const char* pattern)
{
	return [=](cmd::in& source, cmd::out& yield)
	{
		grep(pattern, true)(source, yield);
	};
}

cmd::link contain(const std::string& in)
{
	return [=](cmd::in& source, cmd::out& yield)
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
	return [=](cmd::in& source, cmd::out& yield)
	{
		// TODO: use std::unique
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

cmd::link sort(bool stable = false)
{
	return [=](cmd::in& source, cmd::out& yield)
	{
		std::vector<std::string> sorted;
		for (auto s : source)
		{
			sorted.emplace_back(s);
		}
		if(!stable)
		{
			std::sort(sorted.begin(), sorted.end());
		}
		else
		{
			std::stable_sort(sorted.begin(), sorted.end());
		}
		for (const auto& s : sorted)
		{
			yield(s);
		}
	};
}

cmd::link cut(int field, const char* delim = " ")
{
	return [=](cmd::in& source, cmd::out& yield)
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
	return [=](cmd::in& source, cmd::out& yield)
	{
		for (auto s : source)
		{
			std::stringstream ss;
			ss << delim << s << delim;
			yield(ss.str());
		}
	};
}

cmd::link join(const char* delim = " ", int grouping = 0)
{
	/*
	use grouping=0 for disabling groups.
	use grouping>0 for yield end line each "grouping" times
	*/
	return [=](cmd::in& source, cmd::out& yield)
	{
		std::stringstream ss;
		int i = 0;
		for (auto s : source)
		{
			if(i != 0)
				ss << delim << s;
			else
				ss << s;
			if((grouping > 0) && (i % grouping == 0))
				ss << '\n';
			++i;
		}
		yield(ss.str());
	};
}

cmd::link split(const std::string& text, const char* delim = " ", bool keep_empty=true)
{
	return [=](cmd::in&, cmd::out& yield)
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
	return [=](cmd::in& source, cmd::out& yield)
	{
		for (auto s : source)
		{
			split(s, delim, keep_empty)(source, yield);
		}
	};
}

cmd::link assert_string(const std::string& matching)
{
	return [=](cmd::in& source, cmd::out& yield)
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
	return [=](cmd::in& source, cmd::out& yield)
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

cmd::link count()
{
	return [=](cmd::in& source, cmd::out& yield)
	{
		size_t total = 0;
		for (auto s : source)
		{
			++total;
		}
		yield(std::to_string(total));
	};
}
	
cmd::link assert_count(size_t expected)
{
	return [=](cmd::in& source, cmd::out& yield)
	{
		size_t total = 0;
		for (auto s : source)
		{
			++total;
			yield(s);
		}
		if(expected != total)
		{
			std::stringstream ss;
			ss << "<assert_count> error count: " << total << ", but expected value: " << expected << std::endl;
			throw std::runtime_error(ss.str());
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
	return [=](cmd::in&, cmd::out& yield)
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

cmd::link out(std::string& str)
{
	return [&](cmd::in& source, cmd::out& yield)
	{
		for (auto s : source)
		{
			str = s;
			yield(s);
		}
	};
}
	
cmd::link out(int& number)
{
	return [&](cmd::in& source, cmd::out& yield)
	{
		for (auto s : source)
		{
			number = std::stoi(s);
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
			std::cout << s << "\n";
			yield(s);
		}
	};
}

cmd::link err()
{
	return [&](cmd::in& source, cmd::out& yield)
	{
		for (auto s : source)
		{
			std::cerr << s << "\n";
			yield(s);
		}
	};
}

template <std::ctype_base::mask mask>
class IsNot
{
	std::locale myLocale;       // To ensure lifetime of facet...
	std::ctype<char> const* myCType;
public:
	IsNot( std::locale const& l = std::locale() )
		: myLocale( l )
		, myCType( &std::use_facet<std::ctype<char> >( l ) )
	{
	}
	bool operator()( char ch ) const
	{
		return ! myCType->is( mask, ch );
	}
};

using IsNotSpace = IsNot<std::ctype_base::space>;

cmd::link lstrip()
{
	return [&](cmd::in& source, cmd::out& yield)
	{
		for (auto s : source)
		{
			std::string buf(s);
			buf.erase(buf.begin(), std::find_if( buf.begin(), buf.end(), IsNotSpace() ) );
			yield(buf);
		}
	};
}

cmd::link rstrip()
{
	return [&](cmd::in& source, cmd::out& yield)
	{
		for (auto s : source)
		{
			std::string buf(s);
			buf.erase(std::find_if(buf.rbegin(), buf.rend(), IsNotSpace()).base(), buf.end());
			yield(buf);
		}
	};
}

cmd::link strip()
{
	return [=](cmd::in& source, cmd::out& yield)
	{
		/*
		lstrip()(source, yield);
		rstrip()(source, yield);
		*/
		for (auto s : source)
		{
			auto right = std::find_if( s.rbegin(), s.rend(), IsNotSpace() ).base();
			auto left = std::find_if(s.begin(), right, IsNotSpace() );
			yield( std::string( left, right ) );
		}
	};
}

cmd::link run(const std::string& cmd)
{
	char buff[BUFSIZ];
	return [cmd, &buff](cmd::in& source, cmd::out& yield)
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
			yield(std::string(buff));
		}
		pclose(in);
	};
}

cmd::link run()
{
	return [&](cmd::in& source, cmd::out& yield)
	{
		for (auto s : source)
		{
			// use run("echo $1 $2 $3")
			run(s)(source, yield);
		}
	};
}

}

#endif
