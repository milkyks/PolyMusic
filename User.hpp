#ifndef USER_HPP_INCLUDED
#define USER_HPP_INCLUDED

#include <iostream>
#include <string>
#include <vector>
#include <pqxx/pqxx>
#include <tgbot/tgbot.h>

using namespace std;
using namespace pqxx;

class User
{
public:
  User(const id_t &id);
  void init(transaction_base &db, TgBot::Bot &bot);
  void signIn(transaction_base &db, TgBot::Bot &bot, const string& login, const string& password);
  string runApiMethod(const string &method, const map<string, string> &params = {}, const string &filter = "") const;
  vector<id_t> getAudio() const;
  void sendAudio(const vector<id_t> &ids, TgBot::Bot &bot) const;
  void synchronizeAudio(transaction_base &db, TgBot::Bot &bot);
  void signOut(transaction_base &db);
  string getName() const;
  size_t getAudioSize() const;
  friend ostream& operator<<(ostream& os, const User& user);

private:
  id_t tg_id_;
  id_t vk_id_;
  string cookies_;
  vector<id_t> audio_;
};

ostream& operator<<(ostream& os, const User& user);

#endif
