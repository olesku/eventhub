#include "catch.hpp"
#include <vector>
#include <utility>
#include "topic_manager.hpp"

using namespace std;
using namespace eventhub;

TEST_CASE("is_valid_topic_filter", "[topic_manager]") {
  SECTION("Filter cannot be empty") {
    REQUIRE(topic_manager::is_valid_topic_filter("") == false);
  }

  SECTION("Filter cannot start with /") {
    REQUIRE(topic_manager::is_valid_topic_filter("/") == false);
  }

  SECTION("Topic filter cannot include both # and + at the same time.") {
    REQUIRE(topic_manager::is_valid_topic_filter("test/+/#") == false);
    REQUIRE(topic_manager::is_valid_topic_filter("test/#/+") == false);
    REQUIRE(topic_manager::is_valid_topic_filter("#+") == false);
  }

  SECTION("Topic filter cannot include illegal characters") {
    const string illegal_chars = "&%~/(){}[]";
    REQUIRE(topic_manager::is_valid_topic_filter(illegal_chars) == false);
  }

  SECTION("+ is only valid in within two separators /+/") {
    REQUIRE(topic_manager::is_valid_topic_filter("test/+") == false);
    REQUIRE(topic_manager::is_valid_topic_filter("test/+/te+st") == false);
    REQUIRE(topic_manager::is_valid_topic_filter("+") == false);
    REQUIRE(topic_manager::is_valid_topic_filter("temperature/+/sensor1") == true);
  }

  SECTION("# must be preceded by a / if it's not the only filter.") {
    REQUIRE(topic_manager::is_valid_topic_filter("test#") == false);
    REQUIRE(topic_manager::is_valid_topic_filter("test/#") == true);
  }

  SECTION("# must be last character in filter") {
    REQUIRE(topic_manager::is_valid_topic_filter("test/#ing") == false);
    REQUIRE(topic_manager::is_valid_topic_filter("test/#") == true);
  }

  SECTION("Topic filter # should be valid") {
    REQUIRE(topic_manager::is_valid_topic_filter("#") == true);
  }

  SECTION("Full path is a valid filter") {
    REQUIRE(topic_manager::is_valid_topic_filter("temperature/room1/sensor1") == true);
  }
}

TEST_CASE("is_filter_matched", "[topic_manager]") {
  vector<pair<string, string>> should_match;
  vector<pair<string, string>> should_not_match;

  #define SHOULD_MATCH(s1, s2) should_match.push_back(make_pair<string, string>(s1, s2))
  #define SHOULD_NOT_MATCH(s1, s2) should_not_match.push_back(make_pair<string, string>(s1, s2))

  SHOULD_MATCH("temperature/kitchen/sensor1", "temperature/kitchen/sensor1");
  SHOULD_MATCH("temperature/+/sensor1", "temperature/kitchen/sensor1");
  SHOULD_MATCH("temperature/#", "temperature/kitchen/sensor1");
  SHOULD_MATCH("temperature/kitchen/#", "temperature/kitchen/sensor1");
  SHOULD_MATCH("#", "temperature/kitchen/sensor1");

  SHOULD_NOT_MATCH("temperature/kitchen/sensor1", "temperature/kitchen/sensor2");
  SHOULD_NOT_MATCH("temperature/+/sensor1", "temperature/livingroom/#");
  SHOULD_NOT_MATCH("somethingelse/+/sensor1", "temperature/livingroom/#");
  SHOULD_NOT_MATCH("somethingelse/#", "temperature/livingroom/#");

  SECTION("Should match") {
    for (auto p : should_match) {
      INFO(p.second << " should be matched by filter " << p.first);
      REQUIRE(topic_manager::is_filter_matched(p.first, p.second));
    }
  }

  SECTION("Should not match") {
    for (auto p : should_not_match) {
      INFO(p.second << " should NOT be matched by filter " << p.first);
      REQUIRE(topic_manager::is_filter_matched(p.first, p.second) == false);
    }
  }
}
