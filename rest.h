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
#include <curl/curl.h>
//
#include "channel.h"

size_t writeFunction(void *ptr, size_t size, size_t nmemb, std::string* data)
{
	data->append((char*) ptr, size * nmemb);
	return size * nmemb;
}

class curl_handler
{
public:
	explicit curl_handler()
	{
		_curl = curl_easy_init();
		if (_curl)
		{
			curl_easy_setopt(_curl, CURLOPT_NOPROGRESS, 1L);
			curl_easy_setopt(_curl, CURLOPT_USERAGENT, "cppunix/1.0.0");
			curl_easy_setopt(_curl, CURLOPT_MAXREDIRS, 50L);
			curl_easy_setopt(_curl, CURLOPT_TCP_KEEPALIVE, 1L);
			curl_easy_setopt(_curl, CURLOPT_WRITEFUNCTION, writeFunction);
		}
	}

	~curl_handler()
	{
		if (_curl)
		{
			curl_easy_cleanup(_curl);
			_curl = NULL;
		}
	}

	bool get(const std::string& url, std::string& response_string, std::string& header_string)
	{
		if(_curl)
		{
			std::cout << "get from " << url << std::endl;
			curl_easy_setopt(_curl, CURLOPT_WRITEDATA, &response_string);
			curl_easy_setopt(_curl, CURLOPT_HEADERDATA, &header_string);
			curl_easy_setopt(_curl, CURLOPT_URL, url.c_str());
			// curl_easy_setopt(curl, CURLOPT_USERPWD, "user:pass");
			// long response_code;
			// curl_easy_getinfo(_curl, CURLINFO_RESPONSE_CODE, &response_code);
			// double elapsed;
			// curl_easy_getinfo(_curl, CURLINFO_TOTAL_TIME, &elapsed);
			// char* url;
			// curl_easy_getinfo(_curl, CURLINFO_EFFECTIVE_URL, &url);

			return curl_easy_perform(_curl) == 0;
		}
		return false;
	}
protected:
	CURL* _curl;
};

namespace cu {

static curl_handler curl;
using json = nlohmann::json;
using ch_json = cu::channel<json>;

ch_json::link get(const std::string& url)
{
	return [&](ch_json::in&, ch_json::out& yield)
	{
		std::string response_string;
		std::string header_string;
		bool response = curl.get(url, response_string, header_string);
		std::cout << "header = " << header_string << std::endl;
		std::cout << "response = " << response_string << std::endl;
		if(response)
		{
			std::cout << "response = ok" << std::endl;
			json root;
			std::istringstream str(response_string);
			str >> root;
			if(root.is_array())
			{
				for (auto& element : root)
				{
					yield(element);
				}
			}
			else
			{
				yield(root);
			}
		}
		else
		{
			std::cout << "invalid response: " << response << std::endl;
		}
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
				auto jsn = *s;
				get( jsn["url"].get<std::string>() )(source, yield);
			}
		}
	};
}

}

#endif

