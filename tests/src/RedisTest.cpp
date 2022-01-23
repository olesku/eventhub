#include "Common.hpp"
#include "Config.hpp"
#include "catch.hpp"
#include "jwt/json/json.hpp"
#include <future>
#include <vector>

#include "Redis.hpp"
#include "Util.hpp"

using namespace eventhub;

TEST_CASE("Test redis", "[Redis") {
    ConfigMap cfgMap = {
    { "redis_host",               ConfigValueType::STRING, "localhost",     ConfigValueSettings::REQUIRED },
    { "redis_port",               ConfigValueType::INT,    "6379",          ConfigValueSettings::REQUIRED },
    { "redis_password",           ConfigValueType::STRING, "",              ConfigValueSettings::OPTIONAL },
    { "redis_prefix",             ConfigValueType::STRING, "eventhub_test", ConfigValueSettings::OPTIONAL },
    { "redis_pool_size",          ConfigValueType::INT,    "5",             ConfigValueSettings::REQUIRED },
    { "max_cache_length",         ConfigValueType::INT,    "1000",          ConfigValueSettings::REQUIRED },
    { "max_cache_request_limit",  ConfigValueType::INT,    "100",           ConfigValueSettings::REQUIRED },
    { "default_cache_ttl",        ConfigValueType::INT,    "60",             ConfigValueSettings::REQUIRED },
    { "enable_cache",             ConfigValueType::BOOL,   "true",          ConfigValueSettings::REQUIRED }
  };

  Config cfg(cfgMap);
  cfg.load();

  eventhub::Redis redis(cfg);

  SECTION("Ping connection") {
    bool connected = true;

    try {
      redis.connection()->ping();
    } catch (std::exception& e) {
      connected = false;
    }

    REQUIRE(connected);
  }

  GIVEN("That we increase pub count for test/channel1") {
    redis.connection()->hdel("eventhub_test:pub_count", "test/channel1");
    redis._incrTopicPubCount("test/channel1");

    THEN("Hashentry eventhub_test.test/channel1 should be larger than 0") {
      auto countStr = redis.connection()->hget("eventhub_test:pub_count", "test/channel1");
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

    redis.cacheMessage("test/channel1", "Test 1", "petter@testmann.no", 0, 0);
    redis.cacheMessage("test/channel1", "Test 2", "petter@testmann.no", 0, 0);
    redis.cacheMessage("test/channel1", "Test 3", "petter@testmann.no", 0, 0);
    redis.cacheMessage("test/channel1", "Test 4", "petter@testmann.no", 0, 0);

    redis.cacheMessage("test/channel2", "Test 5", "petter@testmann.no", 0, 0);
    redis.cacheMessage("test/channel2", "Test 6", "petter@testmann.no", 0, 0);
    redis.cacheMessage("test/channel2", "Test 7", "petter@testmann.no", 0, 0);
    redis.cacheMessage("test/channel2", "Test 9", "", 0, 0);
    auto msgId = redis.cacheMessage("test/channel2", "Test 8", "petter@testmann.no", 0, 0);

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
    redis.psubscribe("*", [&msgRcvd](const std::string& pattern, const std::string& topic, const std::string& msg) {
      REQUIRE(pattern.compare("eventhub_test:*") == 0);
      REQUIRE((topic.compare("test/topic1") == 0 || topic.compare("test/topic2") == 0));

      msgRcvd++;
    });

    auto t = std::async(std::launch::async, [&redis]() {
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      redis.publishMessage("test/topic1", "31337", "Test", "test@user.com");
      redis.publishMessage("test/topic2", "31337", "Test", "test@user.com");
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

  GIVEN("That we send in 1000-1:10000:petter@testmann.no to CacheItemMeta") {
    auto p = CacheItemMeta{"1000-1:10000:petter@testmann.no"};

    THEN("We expect values to be parsed correctly") {
      REQUIRE(p.origin() == "petter@testmann.no");
      REQUIRE(p.id() == "1000-1");
      REQUIRE(p.expireAt() == 10000);
    }
  }

  GIVEN("That we send in 1000-1:10000 to CacheItemMeta") {
    auto p = CacheItemMeta{"1000-1:10000"};

    THEN("We expect values to be parsed correctly") {
      REQUIRE(p.origin() == "");
      REQUIRE(p.id() == "1000-1");
      REQUIRE(p.expireAt() == 10000);
    }
  }

  GIVEN("That we create a new CacheItemMeta{1000-1, 10000, petter@testmann.no}") {
    auto item = CacheItemMeta{"1000-1", 10000, "petter@testmann.no"};

    THEN("toStr should return 1000-1:10000:petter@testmann.no") {
      REQUIRE(item.toStr() == "1000-1:10000:petter@testmann.no");
    }
  }

  GIVEN("That we create a new CacheItemMeta{1000-1, 10000, ""}") {
    auto item = CacheItemMeta{"1000-1", 10000, ""};

    THEN("toStr should return 1000-1:10000") {
      REQUIRE(item.toStr() == "1000-1:10000");
    }
  }

  GIVEN("That we want to get cached elements since a given message id") {
    nlohmann::json res;
    std::vector<std::string> cacheIds;

    auto firstId = redis.cacheMessage("test/topic1", "31337", "petter@testmann.no");
    for (unsigned int i = 0; i < 10; i++) {
      cacheIds.push_back(redis.cacheMessage("test/topic1", "31337", "petter@testmann.no"));
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
