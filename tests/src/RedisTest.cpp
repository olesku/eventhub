#include <future>
#include "catch.hpp"
#include "Common.hpp"
#include "jwt/json/json.hpp"

#define private public
#include "Redis.hpp"
#undef private

using namespace std;
using namespace eventhub;

eventhub::Redis redis("127.0.0.1");

TEST_CASE("Test redis", "[Redis") {
  SECTION("Ping connection") {
    bool connected = true;

    try {
      redis._redisInstance->ping();
    } catch (std::exception& e) {
      connected = false;
    }

    REQUIRE(connected);
  }

  GIVEN("That we increase pub count for test/channel1") {
    redis._redisInstance->hdel("eventhub_test.pub_count", "test/channel1");
    redis.setPrefix("eventhub_test");
    redis._incrTopicPubCount("test/channel1");

    THEN("Hashentry eventhub_test.test/channel1 should be larger than 0") {
      auto countStr = redis._redisInstance->hget("eventhub_test.pub_count", "test/channel1");
      int count     = 0;

      try {
        count = std::stoi(countStr.value());
      } catch (std::exception& e) {
        count = 0;
      }

      REQUIRE(count == 1);
    }

    THEN("GetTopicsSeen for pattern test/# should include test/channel1") {
      auto seenTopics = redis._getTopicsSeen("test/#");

      bool seen = false;
      for (auto& topic : seenTopics) {
        if (topic.compare("test/channel1") == 0) {
          seen = true;
        }
      }

      REQUIRE(seen);
    }
  }

  GIVEN("If we cache some items") {
    redis.cacheMessage("test/channel1", "Test 1");
    redis.cacheMessage("test/channel1", "Test 2");
    redis.cacheMessage("test/channel1", "Test 3");
    redis.cacheMessage("test/channel1", "Test 4");

    redis.cacheMessage("test/channel2", "Test 5");
    redis.cacheMessage("test/channel2", "Test 6");
    redis.cacheMessage("test/channel2", "Test 7");
    auto msgId = redis.cacheMessage("test/channel2", "Test 8");

    THEN("Dump cache") {
      redis.getCache("test/#", "0", 0);
      REQUIRE(true);
    }
  }

  GIVEN("That we publish 2 messages") {
    unsigned int msgRcvd = 0;
    redis.psubscribe("*", [&msgRcvd](std::string pattern, std::string topic, std::string msg) {
      REQUIRE(pattern.compare("eventhub_test.*") == 0);
      REQUIRE((topic.compare("test/topic1") == 0 || topic.compare("test/topic2") == 0));

      msgRcvd++;
    });

    auto t = std::async(std::launch::async, [](){
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      redis.publishMessage("test/topic1", "31337", "Test");
      redis.publishMessage("test/topic2", "31337", "{}");
      return true;
    });

    THEN("Our registered callback should receive both of them") {
      // For some reason we have to call this one time extra for the test to pass.
      redis.consume(); // Probably the subscribe event.
      redis.consume();
      redis.consume();

      t.wait();
      REQUIRE(msgRcvd == 2);
    }
  }
}
