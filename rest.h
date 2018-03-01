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
			curl_easy_setopt(_curl, CURLOPT_USERAGENT, "curl/7.42.0");
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
		auto response = curl.get(url, response_string, header_string);
		// std::cout << "url " << url << std::endl;
		// std::cout << "response " << response << std::endl;
		// std::cout << "response_string " << response_string << std::endl;
		// std::cout << "header_string " << header_string << std::endl;
		if(response)
		{
			json root;
			std::istringstream str(response_string);
			str >> root;
			for (auto& element : root)
			{
				yield(element);
			}
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
				get( (*s)["url"].get<std::string>() )(source, yield);
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

