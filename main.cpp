#include <iostream>
#include <stdexcept>
#include <memory>
#include <pqxx/pqxx>
#include <tgbot/tgbot.h>
#include "User.hpp"

using namespace std;
using namespace pqxx;

bool sigintGot = false;
bool signedIn = false;

int main()
{
  try
  {
    connection db_connection("dbname=polymusic");
    work db_work(db_connection);

    TgBot::Bot bot("your access token");
    User user(244270350);

    bot.getEvents().onCommand("start", [&bot, &db_work, &user](TgBot::Message::Ptr message)
    {
      if (!signedIn)
      {
        user.init(db_work, bot);
        signedIn = true;
      }
      
      cout << user << "\n";
    });

    bot.getEvents().onCommand("synchronize", [&bot, &user, &db_work](TgBot::Message::Ptr message)
    {
      user.synchronizeAudio(db_work, bot);
    });

    /*bot.getEvents().onCommand("signOut", [&user, &db_work](TgBot::Message::Ptr message)
    {
      user.signOut(db_work);
    });*/

    signal(SIGINT, [](int s)
    {
      sigintGot = true;
    });

    TgBot::TgLongPoll longPoll(bot);

    while (!sigintGot)
    {
      longPoll.start();
    }

    // disconnect db
  }
  catch (const exception& e)
  {
    cerr << e.what() << "\n";
    return 1;
  }

  return 0;
}
