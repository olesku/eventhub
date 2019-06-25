#include "catch.hpp"
#include "common.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#define private public
#include "redis_subscriber.hpp"
#undef private

using namespace std;
using namespace eventhub;

typedef struct {
  string pattern;
  string channel;
  string data;
} pmessage_t;

string generate_pmessage(const string& pattern, const string& topic, const string& data) {
  stringstream s;

  s << "*4\r\n";
  s << "$8\r\n";
  s << "pmessage\r\n";
  s << "$" << pattern.length() << "\r\n";
  s << pattern << "\r\n";
  s << "$" << topic.length() << "\r\n";
  s << topic << "\r\n";
  s << "$" << data.length() << "\r\n";
  s << data << "\r\n";

  return s.str();
}

RedisSubscriber rs;

TEST_CASE("Test parser", "[RedisSubscriber") {
  GIVEN("3 topics") {
    auto messages = GENERATE(as<pmessage_t>{},
                             pmessage_t{"eventhub.*", "eventhub.testchannel1", "Some data here"},
                             pmessage_t{"eventhub.longertestchanne2", "eventhub.longertestchanne2", "Some more data here"},
                             pmessage_t{"eventhub.*", "eventhub.longesttestchannel3", "Even more data here! Even more data here! Even more data here! MOAR DATA"},
                             pmessage_t{"*", "channel4", "Less data"});

    bool channel_name_matched = false;
    bool pattern_matched      = false;
    bool data_matched         = false;

    rs.psubscribe(messages.pattern, [&](const std::string& pattern, const std::string& channel, const std::string& data) {
      pattern_matched      = (pattern.compare(messages.channel) == 0);
      channel_name_matched = (channel.compare(messages.channel) == 0);
      data_matched         = (data.compare(messages.data) == 0);
    });

    auto wire_data = generate_pmessage(messages.pattern, messages.channel, messages.data);

    WHEN("Parsed without buffering / data split") {
      rs._parse(wire_data.c_str());

      THEN("Channel name and data should be correct in callback") {
        REQUIRE(channel_name_matched == true);
        REQUIRE(data_matched == true);
      }

      THEN("Parser buffer should be empty and state correct") {
        REQUIRE(rs._parse_buffer.length() == 0);
      }

      THEN("Parser state should be READ_RESP") {
        REQUIRE(rs._get_parser_state() == 0);
      }
    }

    WHEN("Parsed with 5 char buffering at a time") {
      REQUIRE(!channel_name_matched);
      REQUIRE(!data_matched);

      for (auto i = 0; i < wire_data.length(); i += 5) {
        auto buf = wire_data.substr(i, 5);
        rs._parse(buf.c_str());
      }

      THEN("Channel name and data should be correct in callback") {
        REQUIRE(channel_name_matched == true);
        REQUIRE(data_matched == true);
      }

      THEN("Parser buffer should be empty and state correct") {
        REQUIRE(rs._parse_buffer.length() == 0);
      }

      THEN("Parser state should be READ_RESP") {
        REQUIRE(rs._get_parser_state() == 0);
      }
    }
  }
}

TEST_CASE("Rest buffer test", "[RedisSubscriber") {
  SECTION("Check that we don't have a restbuffer") {
    auto data0 = "*3\r\n"
                 "$10\r\n"
                 "psubscribe\r\n"
                 "$6\r\n"
                 "test.*\r\n"
                 ":1\r\n";

    auto data1 = "*4\r\n"
                 "$8\r\n"
                 "pmessage\r\n"
                 "$6\r\n"
                 "test.*\r\n"
                 "$10\r\n"
                 "test.test1\r\n"
                 "$2\r\n"
                 "Yo\r\n";

    auto data2 = "*4\r\n"
                 "$8\r\n"
                 "pmessage\r\n"
                 "$6\r\n"
                 "test.*\r\n"
                 "$10\r\n"
                 "test.test1\r\n"
                 "$20\r\n"
                 "sdfaasdfasdfasdfasdf\r\n";

    auto data3 = "*4\r\n"
                 "$8\r\n"
                 "pmessage\r\n"
                 "$6\r\n"
                 "test.*\r\n"
                 "$10\r\n"
                 "test.test1\r\n"
                 "$2\r\n"
                 "Yo\r\n";

    rs.psubscribe("test.*", [&](const std::string& pattern, const std::string& channel, const std::string& data) {

    });

    rs._parse(data0);
    rs._parse(data1);
    rs._parse(data2);
    rs._parse(data3);

    REQUIRE(rs._parse_buffer.length() == 0);
    REQUIRE(rs._get_parser_state() == 0);
  }
}
