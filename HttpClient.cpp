#include <iostream>
#include <stdexcept>
#include <string>
#include <regex>
#include <map>
#include <curl/curl.h>

#include "iconvpp/iconv.hpp"
#include "HttpClient.hpp"

using namespace std;

size_t HttpClient::writeCurlToString(void *contents, size_t size, size_t nmemb, string *s)
{
  size_t newLength = size*nmemb;
  size_t oldLength = s->size();
  
  try
  {
    s->resize(oldLength + newLength);
  }
  catch (bad_alloc &e)
  {
    cerr << e.what() << "\n";
    return 1;
  }

  copy((char*)contents, (char*)contents + newLength, s->begin() + oldLength);
  return size * nmemb;
}

string HttpClient::makeRequest(const string &url, const string &cookies, const string &post_data)
{
  CURL *curl;
  CURLcode res;
 
  curl = curl_easy_init();
  string s = "";
  
  if (curl)
  {
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HEADER, 1);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCurlToString);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &s);

    struct curl_slist *headers = NULL;

    headers = curl_slist_append(headers,
      "accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8");
    headers = curl_slist_append(headers,
      "content-type: application/x-www-form-urlencoded");
    headers = curl_slist_append(headers, 
      "user-agent: Mozilla/5.0 (X11; Linux x86_64; rv:52.0) Gecko/20100101 Firefox/52.0");

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    if (!cookies.empty())
    {
      curl_easy_setopt(curl, CURLOPT_COOKIE, cookies.c_str());
    }

    if (!post_data.empty())
    {
      curl_easy_setopt(curl, CURLOPT_POST, 1);
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
    }
 
    res = curl_easy_perform(curl);

    if (res != CURLE_OK)
    {
      throw runtime_error(curl_easy_strerror(res));
    }

    curl_easy_cleanup(curl);
    delete headers;
  }

  iconvpp::converter conv("utf-8", "windows-1251", true, 1024);
  
  string out = "";
  conv.convert(s, out);

  return out;
}

void HttpClient::saveFile(const string &url, const string &file_name)
{
  CURL* curl = curl_easy_init();

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str()) ;

  FILE* file = fopen(file_name.c_str(), "w");

  curl_easy_setopt(curl, CURLOPT_WRITEDATA, file) ;
  curl_easy_perform(curl);
  curl_easy_cleanup(curl);

  fclose(file);
}

string HttpClient::getCookies(const string& data)
{
  istringstream iss(data);
  string line;

  regex re(R"(Set-Cookie: (.*?);)");
  smatch match;

  string cookies;

  while (getline(iss, line, '\n'))
  {
    if (regex_search(line, match, re))
    {
      cookies += match[1].str() + ";";
    }
  }

  return cookies;
}

string HttpClient::buildQuery(const map<string, string> &params)
{
  string out;

  for (auto param = params.begin(); param != params.end(); ++param)
  {
    out += param->first + "=" + param->second;

    if (param != --params.end())
    {
      out += "&";
    }
  }

  return out;
}

string HttpClient::encodeUrl(const string &s)
{
  CURL *curl = curl_easy_init();
  char *encoded_str = curl_easy_escape(curl, s.c_str(), s.size());
  
  string out(encoded_str);
  curl_free(encoded_str);

  return out;
}
