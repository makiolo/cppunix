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
	// data->append((char*) ptr, size * nmemb);
	// return size * nmemb;
    //
	int result = 0;
	if (data != NULL )
	{
		//data->assign(data, size * nmemb);
		*data += (char*)ptr; // Append instead
		result = size * nmemb;
	}
	return result;
}

class curl_handler
{
public:
	explicit curl_handler()
		: _curl(nullptr)
		, _headers_get(nullptr)
		, _headers_post(nullptr)
	{
		curl_global_init(CURL_GLOBAL_ALL);
		_curl = curl_easy_init();
		if (_curl)
		{
			curl_easy_setopt(_curl, CURLOPT_NOPROGRESS, 1L);
			curl_easy_setopt(_curl, CURLOPT_USERAGENT, "cppunix/1.0.0");
			curl_easy_setopt(_curl, CURLOPT_MAXREDIRS, 50L);
			curl_easy_setopt(_curl, CURLOPT_TCP_KEEPALIVE, 1L);
			curl_easy_setopt(_curl, CURLOPT_WRITEFUNCTION, writeFunction);

			// GET
			_headers_get = curl_slist_append(_headers_get, "Accept: application/json");
			_headers_get = curl_slist_append(_headers_get, "Content-Type: application/json");
			_headers_get = curl_slist_append(_headers_get, "charsets: utf-8");

			// POST
			_headers_post = curl_slist_append(_headers_post, "Accept: text/plain");
			_headers_post = curl_slist_append(_headers_post, "Content-Type: application/octet-stream");
		}
	}

	~curl_handler()
	{
		if (_curl)
		{
			curl_slist_free_all(_headers_get);
			curl_slist_free_all(_headers_post);
			curl_easy_cleanup(_curl);
			_curl = NULL;
		}
		curl_global_cleanup();
	}

	bool get(const std::string& url, std::string& response_string, std::string& header_string)
	{
		if(_curl)
		{
			curl_easy_setopt(_curl, CURLOPT_HTTPHEADER, _headers_get);
			curl_easy_setopt(_curl, CURLOPT_WRITEDATA, &response_string);
			curl_easy_setopt(_curl, CURLOPT_HEADERDATA, &header_string);
			curl_easy_setopt(_curl, CURLOPT_URL, url.c_str());
			auto result =  curl_easy_perform(_curl);
			return result == 0;
		}
		return false;
	}

	bool post(const std::string& url, const std::string& request_string, std::string& header_string)
	{
		if(_curl)
		{
			// std::cout << "post to " << url << " with data = " << request_string << std::endl;
			curl_easy_setopt(_curl, CURLOPT_POSTFIELDS, request_string.c_str());
			curl_easy_setopt(_curl, CURLOPT_HTTPHEADER, _headers_post);
			curl_easy_setopt(_curl, CURLOPT_HEADERDATA, &header_string);
			curl_easy_setopt(_curl, CURLOPT_URL, url.c_str());
			auto result =  curl_easy_perform(_curl);
			return result == 0;
		}
		return false;
	}

protected:
	CURL* _curl;
	struct curl_slist* _headers_get;
	struct curl_slist* _headers_post;
};

namespace cu {

static curl_handler curl;
using json = nlohmann::json;
using ch_json = cu::channel<json>;


static bool curl_post(const std::string& url, const std::string& data)
{
	std::string header_string;
	return curl.post(url, data, header_string);
}


ch_json::link get(const std::string& url)
{
	return [&](ch_json::in&, ch_json::out& yield)
	{
		std::string response_string;
		std::string header_string;
		bool response = curl.get(url, response_string, header_string);
		if(response)
		{
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
			else
			{
				// propagate close
				yield(s);
			}
		}
	};
}

ch_json::link post(const std::string& url, const std::string& data)
{
	return [&](ch_json::in&, ch_json::out& yield)
	{
		std::cout << "post ..." << std::endl;
		std::string header_string;
		curl.post(url, data, header_string);
	};
}

ch_json::link post()
{
	return [&](ch_json::in& source, ch_json::out& yield)
	{
		for (auto& s : source)
		{
			if(s)
			{
				auto jsn = *s;
				auto url = jsn["url"].get<std::string>();
				auto data = jsn["data"].get<std::string>();
				post(url, data)(source, yield);
			}
			else
			{
				// propagate close
				yield(s);
			}
		}
	};
}

}

#endif

