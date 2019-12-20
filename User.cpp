#include <iostream>
#include <stdexcept>
#include <string>
#include <regex>
#include <map>
#include <vector>
#include <pqxx/pqxx>
#include <tgbot/tgbot.h>
#include <unistd.h>

#include "json.hpp"
#include "User.hpp"
#include "HttpClient.hpp"

using namespace std;
using namespace HttpClient;
using namespace pqxx;

using json = nlohmann::json;

template <typename T>
static string join(const vector<T> &items, const string &delim)
{
  ostringstream oss;

  if (!items.empty())
  {
    copy(items.begin(), --items.end(), ostream_iterator<T>(oss, delim.c_str()));
    oss << items.back();
  }

  return oss.str();
}

User::User(const id_t &id):
  tg_id_(id),
  vk_id_(0),
  cookies_(""),
  audio_({})
{}

void User::init(transaction_base &db, TgBot::Bot &bot)
{
  result res = db.exec("SELECT * FROM users WHERE tg_id=" + db.quote(tg_id_));

  if (res.empty())
  {
    signIn(db, bot, "login", "password");
  }
  else
  {
    vk_id_ = res[0]["vk_id"].as<id_t>();
    cookies_ = res[0]["cookies"].as<string>();
    
    string audio = res[0]["audio"].as<string>();

    if (!audio.empty())
    {
      istringstream iss(audio);
      string id;

      while (getline(iss, id, ','))
      {
        audio_.push_back(stoi(id));
      }
    }
    else
    {
      audio_ = {};
    }
  }
}

void User::signIn(transaction_base &db, TgBot::Bot &bot, const string& login, const string& password)
{
  string main_page = makeRequest("https://vk.com");
  string cookies = getCookies(main_page);

  smatch ip_h;
  regex_search(main_page, ip_h, regex(R"(name=\"ip_h\" value=\"(.*?)\")"));

  smatch lg_h;
  regex_search(main_page, lg_h, regex(R"(name=\"lg_h\" value=\"(.*?)\")"));

  map<string, string> auth_params
  {
    {"act", "login"},
    {"role", "al_frame"},
    {"_origin", encodeUrl("http://vk.com")},
    {"ip_h", ip_h[1]},
    {"lg_h", lg_h[1]},
    {"email", login},
    {"pass", password}
  };

  string auth = makeRequest("https://login.vk.com/?act=login", cookies, buildQuery(auth_params));

  smatch location;
  regex_search(auth, location, regex(R"(Location\: (.*))"));

  if (!regex_search(location[1].str(), regex(R"(hash=(.*))")))
  {
    throw runtime_error("auth error");
  }

  string new_location = regex_replace(location[1].str(), regex("_http"), "_https");
  string user_page = makeRequest(new_location, cookies);

  smatch user_id;
  regex_search(user_page, user_id, regex(R"(\"uid"\:"([0-9]+)\")"));

  // TODO: проверка безопасности

  vk_id_ = stoi(user_id[1].str());
  cookies_ = getCookies(user_page);
  audio_ = getAudio();

  bot.getApi().sendMessage(tg_id_, "You're signed in as " + getName());

  if (!audio_.empty())
  {
    sendAudio(audio_, bot);
  }

  db.exec("INSERT INTO users(tg_id, vk_id, cookies, audio) "
    "VALUES ("
    + db.quote(tg_id_) + ","
    + db.quote(vk_id_) + ","
    + db.quote(cookies_) + ","
    + db.quote(join(audio_, ",")) + ")");

  db.commit();
}

string User::runApiMethod(const string &method, const map<string, string> &params, const string &filter) const
{
  string dev_page = makeRequest("https://vk.com/dev/execute", cookies_);

  smatch hash;
  regex_search(dev_page, hash, regex(R"(Dev\.methodRun\('(.*)', this\))"));

  string params_str = "";

  if (!params.empty())
  {
    for (auto param = params.begin(); param != params.end(); ++param)
    {
      params_str += "\"" + param->first + "\":" + param->second;

      if (param != --params.end())
      {
        params_str += ", ";
      }
    }

    params_str = "{" + params_str + "}";
  }

  map<string, string> my_params
  {
    {"act", "a_run_method"},
    {"al", "1"},
    {"hash", hash[1]},
    {"method", "execute"},
    {"param_code", encodeUrl("return API." + method + "(" + params_str + ")" + filter + ";")},
    {"param_v", "5.69"}
  };

  string data = makeRequest("https://vk.com/dev.php", cookies_, buildQuery(my_params));
  smatch response;
  
  if (!regex_search(data, response, regex(R"(\{"response":(.*)\})")))
  {
    throw runtime_error("error response");
  }

  return response[0].str();
}

vector<id_t> User::getAudio() const
{
  string response = runApiMethod("audio.get", {}, ".items@.id");

  auto json = json::parse(response);
  vector<id_t> ids = {};

  if (!json["response"].empty())
  {
    for (auto id: json["response"])
    {
      ids.push_back(id);
    }

    reverse(begin(ids), end(ids));
  }

  return ids;
}

void User::sendAudio(const vector<id_t> &ids, TgBot::Bot &bot) const
{
  size_t count = 30;
  size_t offset = 0;

  size_t i = 1;
  
  do
  {
    vector<id_t>::const_iterator first = ids.begin() + offset;
    vector<id_t>::const_iterator last = first + count;
    
    vector<id_t> sub_ids(first, last);

    string audios = "\"";

    for (auto id = sub_ids.begin(); id != sub_ids.end(); ++id)
    {
      audios += to_string(vk_id_) + "_" + to_string(*id);
      audios += (id != --sub_ids.end()) ? "," : "\"";
    }

    string response = runApiMethod("audio.getById", {{"audios", audios}}); // TODO: обход капчи

    auto json = json::parse(response);

    for (auto audio: json["response"])
    {
      cout << (int)(i / (double) ids.size() * (double) 100) << "%: "
        << audio["artist"] << " 一 " << audio["title"] << "\n";

      string url = audio["url"].get<string>();
      
      if (!url.empty())
      { 
        try
        {
          bot.getApi().sendAudio(tg_id_, url, "",
            audio["duration"].get<int>(),
            audio["artist"].get<string>(),
            audio["title"].get<string>(),
            0, NULL, true);

          sleep(1);
        }
        catch (...)
        {
          cerr << "Some wrong while sending audio!\n";
        }
      }
      else
      {
        cerr << "Not available!\n";
      }

      i++;
    }

    offset += count;
  }
  while (offset < ids.size());

  cout << "Audio has been sent!\n";
}

void User::synchronizeAudio(transaction_base &db, TgBot::Bot &bot)
{
  vector<id_t> new_audio = getAudio();
  vector<id_t> new_ids = {};

  for (auto id: new_audio)
  {
    if (find(audio_.begin(), audio_.end(), id) == audio_.end())
    {
      new_ids.push_back(id);
    }
  }

  if (!new_ids.empty())
  {
    sendAudio(new_ids, bot);
  }
  else
  {
    cout << "no new audio\n";
  }

  audio_ = new_audio;

  db.exec("UPDATE users SET audio=" + db.quote(join(audio_, ",")) + " WHERE tg_id=" + db.quote(tg_id_));
  db.commit();
}

void User::signOut(transaction_base &db)
{
  vk_id_ = 0;
  cookies_ = "";
  audio_.clear();

  db.exec("DELETE FROM users WHERE tg_id=" + db.quote(tg_id_));
  db.commit();
}

string User::getName() const
{
  string response = runApiMethod("users.get");
  auto json = json::parse(response);

  return json["response"][0]["first_name"].get<string>() + " "
    + json["response"][0]["last_name"].get<string>();
}

size_t User::getAudioSize() const
{
  return audio_.size();
}

ostream& operator<<(ostream& os, const User& user)
{
  os << "tg id: " << user.tg_id_ << "\n"
    << "vk id: " << user.vk_id_ << "\n"
    << "cookies: " << user.cookies_ << "\n"
    << "audio size: " << user.getAudioSize();

  return os;
}
