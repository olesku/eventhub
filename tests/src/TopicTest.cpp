#include <utility>
#include <vector>
#include <string>

#include "TopicManager.hpp"
#include "catch.hpp"
#include "Config.hpp"

using namespace eventhub;

TEST_CASE("isValidTopicFilter", "[topic_manager]") {
  SECTION("Filter cannot be empty") {
    REQUIRE(TopicManager::isValidTopicFilter("") == false);
  }

  SECTION("Filter cannot start with /") {
    REQUIRE(TopicManager::isValidTopicFilter("/") == false);
  }

  SECTION("Filter cannot end with /") {
    REQUIRE(TopicManager::isValidTopicFilter("test/topic1/") == false);
  }

  SECTION("A # in a topic filter must always be at the end") {
    REQUIRE(TopicManager::isValidTopicFilter("#") == true);
    REQUIRE(TopicManager::isValidTopicFilter("foo/bar/#") == true);
    REQUIRE(TopicManager::isValidTopicFilter("test/#/+") == false);
    REQUIRE(TopicManager::isValidTopicFilter("#+") == false);
    REQUIRE(TopicManager::isValidTopicFilter("test/#/topic") == false);
  }

  SECTION("A combination of + and # is allowed.") {
    REQUIRE(TopicManager::isValidTopicFilter("test/+/#") == true);
  }

  SECTION("Topic filter cannot include illegal characters") {
    const std::string illegalChars = "&%~/(){}[]";
    REQUIRE(TopicManager::isValidTopicFilter(illegalChars) == false);
  }

  SECTION("+ is only valid at the start, in between two separators /+/ or at the end.") {
    REQUIRE(TopicManager::isValidTopicFilter("+") == true);
    REQUIRE(TopicManager::isValidTopicFilter("+/test") == true);
    REQUIRE(TopicManager::isValidTopicFilter("+/test") == true);
    REQUIRE(TopicManager::isValidTopicFilter("temperature/+/sensor1") == true);
    REQUIRE(TopicManager::isValidTopicFilter("test/+") == true);
    REQUIRE(TopicManager::isValidTopicFilter("test/+/te+st") == false);
    //REQUIRE(TopicManager::isValidTopicFilter("+/") == false);
  }

  SECTION("# must be preceded by a / if it's not the only filter.") {
    REQUIRE(TopicManager::isValidTopicFilter("test#") == false);
    REQUIRE(TopicManager::isValidTopicFilter("test/#") == true);
    REQUIRE(TopicManager::isValidTopicFilter("test/#/topic") == false);
    REQUIRE(TopicManager::isValidTopicFilter("test/#") == true);
  }

  SECTION("# must be last character in filter") {
    REQUIRE(TopicManager::isValidTopicFilter("test/#ing") == false);
    REQUIRE(TopicManager::isValidTopicFilter("test/#") == true);
  }

  SECTION("Topic filter # should be valid") {
    REQUIRE(TopicManager::isValidTopicFilter("#") == true);
  }

  SECTION("Full path is not a valid filter") {
    REQUIRE(TopicManager::isValidTopicFilter("temperature/room1/sensor1") == false);
  }
}

TEST_CASE("isValidTopic", "[topic_manager]") {
  SECTION("Topic cannot be empty") {
    REQUIRE(TopicManager::isValidTopic("") == false);
  }

  SECTION("Topic cannot start with /") {
    REQUIRE(TopicManager::isValidTopic("/") == false);
  }

  SECTION("Topic cannot end with /") {
    REQUIRE(TopicManager::isValidTopic("test/topic1/") == false);
  }

  SECTION("Full path is a valid topic") {
    REQUIRE(TopicManager::isValidTopic("temperature/room1/sensor1") == true);
  }

  SECTION("A topic filter with '#' is not a valid topic") {
    REQUIRE(TopicManager::isValidTopic("temperature/#") == false);
  }

  SECTION("A topic filter with '+' is not a valid topic") {
    REQUIRE(TopicManager::isValidTopic("temperature/+/sensor1") == false);
  }

  SECTION("Topic cannot include illegal characters") {
    const std::string illegalChars = "&%~/(){}[]";
    REQUIRE(TopicManager::isValidTopic(illegalChars) == false);
  }

  SECTION("# is a valid catch all topic") {
    REQUIRE(TopicManager::isValidTopicFilter("#"));
    REQUIRE(TopicManager::isFilterMatched("#", "foo/bar/baz"));
    REQUIRE(TopicManager::isFilterMatched("#", "foo"));
  }
}

TEST_CASE("isFilterMatched", "[topic_manager]") {
  std::vector<std::pair<std::string, std::string>> should_match;
  std::vector<std::pair<std::string, std::string>> should_not_match;

#define SHOULD_MATCH(s1, s2) should_match.push_back(std::make_pair<std::string, std::string>(s1, s2))
#define SHOULD_NOT_MATCH(s1, s2) should_not_match.push_back(std::make_pair<std::string, std::string>(s1, s2))

  SHOULD_MATCH("temperature/kitchen/sensor1", "temperature/kitchen/sensor1");
  SHOULD_MATCH("temperature/+/sensor1", "temperature/kitchen/sensor1");
  SHOULD_MATCH("temperature/#", "temperature/kitchen/sensor1");
  SHOULD_MATCH("temperature/kitchen/#", "temperature/kitchen/sensor1");
  SHOULD_MATCH("temperature/kitchen/#", "temperature/kitchen");
  SHOULD_MATCH("#", "temperature/kitchen/sensor1");
  SHOULD_MATCH("v1/+/events/+/supporters/baz", "v1/foo/events/bar/supporters/baz");
  SHOULD_MATCH("v1/+/events/+/supporters/+", "v1/foo/events/bar/supporters/baz");
  SHOULD_MATCH("v1/+/events/+/supporters/+", "v1/foo/events/bar/supporters/baz");
  SHOULD_MATCH("+", "foobar");
  SHOULD_MATCH("v1/+/#", "v1/baz/foo/bar");

  SHOULD_NOT_MATCH("temperature/kitchen/sensor1", "temperature/kitchen/sensor2");
  SHOULD_NOT_MATCH("temperature/+/sensor1", "temperature/livingroom/#");
  SHOULD_NOT_MATCH("somethingelse/+/sensor1", "temperature/livingroom/#");
  SHOULD_NOT_MATCH("somethingelse/#", "temperature/livingroom/#");
  SHOULD_NOT_MATCH("test/channel", "test/channel1");
  SHOULD_NOT_MATCH("test", "test1");
  SHOULD_NOT_MATCH("test1/test", "test1/test2");
  SHOULD_NOT_MATCH("test1/test2", "test1/test");
  SHOULD_NOT_MATCH("test1/test222222222", "test1/test");
  SHOULD_NOT_MATCH("test1/foobar", "test1/foo");
  SHOULD_NOT_MATCH("test1/foo", "test1/foobar");
  SHOULD_NOT_MATCH("test1/+/test", "test1/test");
  SHOULD_NOT_MATCH("+", "foobar/baz");
  SHOULD_NOT_MATCH("topic1/#", "topic2");

  SECTION("Should match") {
    for (auto p : should_match) {
      INFO(p.second << " should be matched by filter " << p.first);
      REQUIRE(TopicManager::isFilterMatched(p.first, p.second));
    }
  }

  SECTION("Should not match") {
    for (auto p : should_not_match) {
      INFO(p.second << " should NOT be matched by filter " << p.first);
      REQUIRE(TopicManager::isFilterMatched(p.first, p.second) == false);
    }
  }
}
