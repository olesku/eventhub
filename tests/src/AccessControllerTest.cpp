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
  GIVEN("A valid test token") {
    Config.del("DISABLE_AUTH");
    Config.addBool("DISABLE_AUTH", false);
    std::string token = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJleHAiOjIyMTQ2OTQ5MjMsInN1YiI6Im9sZS5za3Vkc3Zpa0BnbWFpbC5jb20iLCJ3cml0ZSI6WyJ0ZXN0MS8jIiwidGVzdDIvIyIsInRlc3QzLyMiXSwicmVhZCI6WyJ0ZXN0MS8jIiwidGVzdDIvIyIsInRlc3QzLyMiXSwiY3JlYXRlVG9rZW4iOlsidGVzdDEiLCJ0ZXN0MiIsInRlc3QzIl19.FSSecEiStcElHu0AqpmcIvfyMElwKZQUkiO5X_r0_3g";

    AccessController acs;
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
    Config.del("DISABLE_AUTH");
    Config.addBool("DISABLE_AUTH", false);
    AccessController acs;

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
    Config.del("DISABLE_AUTH");
    Config.addBool("DISABLE_AUTH", true);
    AccessController acs;

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
}
