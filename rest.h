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

ch_json::link get(std::string url, std::string params)
{
	RestClient::Connection* conn = new RestClient::Connection(url);
	conn->SetTimeout(5);
	conn->SetUserAgent("restclient/cpp");
	conn->FollowRedirects(true);

	RestClient::HeaderFields headers;
	headers["Accept"] = "application/json";
	conn->SetHeaders(headers);
	conn->AppendHeader("Content-Type", "text/json");

	// // set CURLOPT_SSLCERT
	// conn->SetCertPath(certPath);
	// // set CURLOPT_SSLCERTTYPE
	// conn->SetCertType(type);
	// // set CURLOPT_SSLKEY
	// conn->SetKeyPath(keyPath);

	return [&](ch_json::in&, ch_json::out& yield)
	{
		RestClient::Response res = conn->get(params);
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
				std::string url = "https://api.coinmarketcap.com/v1/ticker/";
				std::string params = "?limit=5";

				// std::string url = "http://samples.openweathermap.org/data/2.5/weather";
				// std::string params = "?q=London,uk&appid=b6907d289e10d714a6e88b30761fae22";

				get(url, params)(source, yield);
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

