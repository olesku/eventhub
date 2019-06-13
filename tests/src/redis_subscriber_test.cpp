#include "catch.hpp"
#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include "redis_subscriber.hpp"

using namespace std;
using namespace eventhub;

typedef struct {
  string topic_name;
  string expected_data;
  unsigned int expected_data_matched;
  unsigned int callback_hit;
} subscription_t;

vector<subscription_t> subscriptions;

string publish_message_raw(const string& topic, const string& data) {
  stringstream s;

  s << "*3\r\n";
  s << "$8\r\n";
  s << "pmessage\r\n";
  s << "$" << topic.length() << "\r\n";
  s << topic << "\r\n";
  s << "$" << data.length() << "\r\n";
  s << data << "\r\n";
  s << "\r\n";

  return s.str();
}

void subscription_callback(const std::string& topic, const std::string& data) {
  for (auto& elm : subscriptions) {
    if (elm.topic_name.compare(topic) == 0) {
      elm.callback_hit++;

      if (elm.expected_data.compare(data) == 0) {
        elm.expected_data_matched++;
      }
    }
  }
}

TEST_CASE("parser", "[redis_subscriber") {
  redis_subscriber rs;

  rs.set_callback(subscription_callback);

  subscriptions.push_back(subscription_t{"eventhub.test1", "Test data topic 1", 0});
  subscriptions.push_back(subscription_t{"eventhub.test2", "Test data topic 2", 0});
  subscriptions.push_back(subscription_t{"eventhub.test3", "Test data topic 3", 0});

  SECTION("Test pub/sub") {
    for (auto& elm : subscriptions) {
      auto data = publish_message_raw(elm.topic_name, elm.expected_data);

      for (unsigned int i = 0; i < data.length(); i+=5) {
        auto str = data.substr(i, 5);
        rs.parse(str.c_str());
      }

      REQUIRE(elm.callback_hit == 1);
      REQUIRE(elm.expected_data_matched  == 1);
    }
  }
}
