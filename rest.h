#ifndef _CU_REST_H_
#define _CU_REST_H_

#include <set>
#include <string>
#include <fstream>
#include <iostream>
#include <sstream>
#include <exception>
#include <vector>
#include <algorithm>
#include <locale>
#include <json.hpp>
#include <restclient-cpp/restclient.h>
//
#include "channel.h"

namespace cu {

using json = nlohmann::json;

// vector<json>
//
// comandos SQL:
//    select *
//    select name, age, address
//
//    where name == 'pepe' and age == 16

using ch_json = cu::channel<json>;

ch_json::link get(const std::string& url)
{
	return [&](ch_json::in&, ch_json::out& yield)
	{
		RestClient::Response res = RestClient::get(url);
		// std::cout << "url = " << url << std::endl;
		// std::cout << res.body << std::endl;
		json root;
		std::istringstream str(res.body);
		str >> root;
		// std::cout << "json = " << root << std::endl;
		yield(root);
	};
}

ch_json::link get()
{
	return [&](ch_json::in& source, ch_json::out& yield)
	{
		for (auto& s : source)
		{
			if(s)
			{
				get(*s)(source, yield);
			}
			else
			{
				yield(s);
			}
		}
	};
}

}

#endif

