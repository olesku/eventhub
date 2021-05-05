#include "Common.hpp"
#include "Config.hpp"
#include "catch.hpp"
#include "jwt/json/json.hpp"
#include <future>
#include <vector>

#include "Redis.hpp"
#include "Util.hpp"

using namespace std;
using namespace eventhub;

TEST_CASE("Test redis", "[Redis") {
    evconfig::ConfigMap cfgMap = {
    { "redis_host",               evconfig::ValueType::STRING, "localhost",     evconfig::ValueSettings::REQUIRED },
    { "redis_port",               evconfig::ValueType::INT,    "6379",          evconfig::ValueSettings::REQUIRED },
    { "redis_password",           evconfig::ValueType::STRING, "",              evconfig::ValueSettings::OPTIONAL },
    { "redis_prefix",             evconfig::ValueType::STRING, "eventhub_test", evconfig::ValueSettings::OPTIONAL },
    { "redis_pool_size",          evconfig::ValueType::INT,    "5",             evconfig::ValueSettings::REQUIRED },
    { "max_cache_length",         evconfig::ValueType::INT,    "1000",          evconfig::ValueSettings::REQUIRED },
    { "max_cache_request_limit",  evconfig::ValueType::INT,    "100",           evconfig::ValueSettings::REQUIRED },
    { "default_cache_ttl",        evconfig::ValueType::INT,    "60",            evconfig::ValueSettings::REQUIRED },
    { "enable_cache",             evconfig::ValueType::BOOL,   "true",          evconfig::ValueSettings::REQUIRED }
  };

  evconfig::Config cfg(cfgMap);
  cfg.load();

  eventhub::Redis redis(cfg);

  SECTION("Ping connection") {
    bool connected = true;

    try {
      redis.getRedisInstance()->ping();
    } catch (std::exception& e) {
      connected = false;
    }

    REQUIRE(connected);
  }

  GIVEN("That we increase pub count for test/channel1") {
    redis.getRedisInstance()->hdel("eventhub_test:pub_count", "test/channel1");
    redis._incrTopicPubCount("test/channel1");

    THEN("Hashentry eventhub_test.test/channel1 should be larger than 0") {
      auto countStr = redis.getRedisInstance()->hget("eventhub_test:pub_count", "test/channel1");
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

    redis.cacheMessage("test/channel1", "Test 1", 0, 0);
    redis.cacheMessage("test/channel1", "Test 2", 0, 0);
    redis.cacheMessage("test/channel1", "Test 3", 0, 0);
    redis.cacheMessage("test/channel1", "Test 4", 0, 0);

    redis.cacheMessage("test/channel2", "Test 5", 0, 0);
    redis.cacheMessage("test/channel2", "Test 6", 0, 0);
    redis.cacheMessage("test/channel2", "Test 7", 0, 0);
    auto msgId = redis.cacheMessage("test/channel2", "Test 8", 0, 0);

    THEN("Cache size should be larger than 0 when requesting a matching pattern") {
      nlohmann::json j;
      size_t cacheSize = redis.getCacheSince("test/#", 0, -1, true, j);
      REQUIRE(cacheSize > 0);
      REQUIRE(j.size() > 0);
    }

    THEN("Cache size should be larger than 0 when requesting the actual topic") {
      nlohmann::json j;
      size_t cacheSize = redis.getCacheSince("test/channel1", 0, -1, false, j);
      REQUIRE(cacheSize > 0);
      REQUIRE(j.size() > 0);
    }
  }

  GIVEN("That we publish 2 messages") {
    unsigned int msgRcvd = 0;
    redis.psubscribe("*", [&msgRcvd](std::string pattern, std::string topic, std::string msg) {
      REQUIRE(pattern.compare("eventhub_test:*") == 0);
      REQUIRE((topic.compare("test/topic1") == 0 || topic.compare("test/topic2") == 0));

      msgRcvd++;
    });

    auto t = std::async(std::launch::async, [&redis]() {
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

  GIVEN("That we send in 1000-1:10000 to _parseIdAndExpireAt") {
    auto p = redis._parseIdAndExpireAt("1000-1:10000");

    THEN("We should get a pair with first element as 1000-1 and second as 10000") {
      REQUIRE(p.first == "1000-1");
      REQUIRE(p.second == 10000);
    }
  }

  GIVEN("That we want to get cached elements since a given message id") {
    nlohmann::json res;
    std::vector<std::string> cacheIds;

    auto firstId = redis.cacheMessage("test/topic1", "31337");
    for (unsigned int i = 0; i < 10; i++) {
      cacheIds.push_back(redis.cacheMessage("test/topic1", "31337"));
    }

    THEN("We should get the expected results back") {
      redis.getCacheSinceId("test/topic1", firstId, 100, false, res);

      unsigned int i = 0;
      for (auto item : res) {
        REQUIRE(cacheIds[i] == item["id"]);
        i++;
      }

      REQUIRE(i == 10);
    }
  }
}
