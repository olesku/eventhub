#include "AccessController.hpp"
#include "Config.hpp"
#include "catch.hpp"
#include <string>

/*
{
  "alg": "HS256",
  "typ": "JWT"
}

{
  "exp": 2214694923,
  "sub": "ole.skudsvik@gmail.com",
  "write": ["test1/#", "test2/#", "test3/#"],
  "read": ["test1/#", "test2/#", "test3/#"],
  "createToken": ["test1", "test2", "test3"]
}

secret: eventhub_secret
*/

using namespace eventhub;

TEST_CASE("Test authorization", "[access_controller]") {
  ConfigMap cfgMap = {
    { "disable_auth", ConfigValueType::BOOL, "", ConfigValueSettings::REQUIRED }
  };

  GIVEN("A valid test token") {
    Config cfg(cfgMap);
    cfg << "disable_auth = 0";
    cfg.load();
    std::string token = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJleHAiOjIyMTQ2OTQ5MjMsInN1YiI6Im9sZS5za3Vkc3Zpa0BnbWFpbC5jb20iLCJ3cml0ZSI6WyJ0ZXN0MS8jIiwidGVzdDIvIyIsInRlc3QzLyMiXSwicmVhZCI6WyJ0ZXN0MS8jIiwidGVzdDIvIyIsInRlc3QzLyMiXSwiY3JlYXRlVG9rZW4iOlsidGVzdDEiLCJ0ZXN0MiIsInRlc3QzIl19.FSSecEiStcElHu0AqpmcIvfyMElwKZQUkiO5X_r0_3g";

    AccessController acs(cfg);
    bool tokenLoaded = acs.authenticate(token, "eventhub_secret");

    WHEN("Loading our token") {
      THEN("The token should be successfully loaded") {
        REQUIRE(tokenLoaded == true);
      }
    }

    WHEN("Testing if we are allowed to subscribe to authorized topics") {
      THEN("We should be allowed to publish to test1/mychannel") {
        REQUIRE(acs.allowSubscribe("test1/mychannel") == true);
      }

      THEN("We should not be allowed to subscribe to /my/very/private/channel") {
        REQUIRE(!acs.allowSubscribe("my/very/private/channel"));
      }
    }

    WHEN("Testing if we are allowed to publish to authorized topics") {
      THEN("We should be allowed to publish to test1/mychannel") {
        REQUIRE(acs.allowPublish("test1/mychannel") == true);
      }

      THEN("We should not be allowed to publish to /my/very/private/channel") {
        REQUIRE(!acs.allowPublish("my/very/private/channel"));
      }
    }
  }

  GIVEN("An invalid token") {
    Config cfg(cfgMap);
    cfg << "disable_auth = 0";
    cfg.load();

    AccessController acs(cfg);

    THEN("Authenticate should fail") {
      bool tokenLoaded = acs.authenticate("eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJleHAiOjIyMTQ2OTQ5MjMsInN1YiI6Im9sZS5za3Vkc3Zpa0BnbWFpbC5jb20iLCJ3cml0ZSI6WyJ0ZXN0MS8jIiwidGVzdDIvIyIsInRlc3QzLyMiXSwicmVhZCI6WyJ0ZXN0MS8jIiwidGVzdDIvIyIsInRlc3QzLyMiXSwiY3JlYXRlVG9rZW4iOlsidGVzdDEiLCJ0ZXN0MiIsInRlc3QzIl19.DMpX-zaifaTt5YbyTqSPjeJfhi8oMSpKsqh2rWGatGY", "Foobar");
      REQUIRE(!tokenLoaded);
      REQUIRE(!acs.isAuthenticated());
    }

    THEN("Subscribe should fail") {
      REQUIRE(!acs.allowSubscribe("test1/mychannel"));
    }

    THEN("Publish should fail") {
      REQUIRE(!acs.allowPublish("test1/mychannel"));
    }
  }

  GIVEN("Authentication is disabled through config") {
    Config cfg(cfgMap);
    cfg << "disable_auth = 1";
    cfg.load();

    AccessController acs(cfg);

    THEN("Authenticate should always return true") {
      REQUIRE(acs.authenticate("Foo", "Bar"));
    }

    THEN("We should be allowed to subscribe to any channel") {
      REQUIRE(acs.allowSubscribe("my/very/private/channel"));
    }

    THEN("We should be allowed to publish to any channel") {
      REQUIRE(acs.allowPublish("my/very/private/channel"));
    }
  }

  GIVEN("We have a rlimit") {
    Config cfg(cfgMap);
    cfg << "disable_auth = 0";
    cfg.load();
    auto token = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiJ1c2VyQGRvbWFpbi5jb20iLCJyZWFkIjpbInRvcGljLyMiLCJ0b3BpYzIvIyJdLCJ3cml0ZSI6WyJ0b3BpYzEvIyJdLCJybGltaXQiOlt7InRvcGljIjoidG9waWMxLyMiLCJpbnRlcnZhbCI6MTAsIm1heCI6MTB9LHsidG9waWMiOiJ0b3BpYzIiLCJpbnRlcnZhbCI6MTAwLCJtYXgiOjEwfV19.i938ZYQL4NR1VUUfrtAwPaivd3cldW6Pegdo9ofpjcE";
    AccessController acs(cfg);
    bool tokenLoaded = acs.authenticate(token, "eventhub_secret");

    THEN("The token should be successfully loaded") {
        REQUIRE(tokenLoaded == true);
    }

    THEN("We should have the correct limits set for topics defined in token.") {
      /*
        "rlimit": [{
          "topic": "topic1/#",
          "interval": 10,
          "max": 10
        },
        {
          "topic": "topic2",
          "interval": 100,
          "max": 10
          }
      */

      REQUIRE(acs.getRateLimitConfig().getRateLimitForTopic("topic1").interval == 10);
      REQUIRE(acs.getRateLimitConfig().getRateLimitForTopic("topic1").max == 10);
      REQUIRE(acs.getRateLimitConfig().getRateLimitForTopic("topic1/foo").interval == 10);
      REQUIRE(acs.getRateLimitConfig().getRateLimitForTopic("topic1/foo").max == 10);
      REQUIRE(acs.getRateLimitConfig().getRateLimitForTopic("topic1/foo/bar").interval == 10);
      REQUIRE(acs.getRateLimitConfig().getRateLimitForTopic("topic1/foo/bar").max == 10);
      REQUIRE(acs.getRateLimitConfig().getRateLimitForTopic("topic2").interval == 100);
      REQUIRE(acs.getRateLimitConfig().getRateLimitForTopic("topic2").max == 10);

      CHECK_THROWS_AS(acs.getRateLimitConfig().getRateLimitForTopic("topic2/foo"), NoRateLimitForTopic);
      CHECK_THROWS_AS(acs.getRateLimitConfig().getRateLimitForTopic("topic3"), NoRateLimitForTopic);
      CHECK_THROWS_AS(acs.getRateLimitConfig().getRateLimitForTopic("topic3"), NoRateLimitForTopic);
    }
  }
}
