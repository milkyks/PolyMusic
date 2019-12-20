#ifndef HTTPCLIENT_HPP_INCLUDED
#define HTTPCLIENT_HPP_INCLUDED

#include <string>
#include <map>

using namespace std;

namespace HttpClient
{
  size_t writeCurlToString(void *contents, size_t size, size_t nmemb, string *s);
  string makeRequest(const string &url, const string &cookies = "", const string &post_data = "");
  void saveFile(const string &url, const string &file_name);
  string getCookies(const string& data);
  string buildQuery(const map<string, string> &params);
  string encodeUrl(const string &s);
};

#endif
