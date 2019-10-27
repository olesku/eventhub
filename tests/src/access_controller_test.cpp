#include "access_controller.hpp"
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
    std::string token = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJleHAiOjIyMTQ2OTQ5MjMsInN1YiI6Im9sZS5za3Vkc3Zpa0BnbWFpbC5jb20iLCJ3cml0ZSI6WyJ0ZXN0MS8jIiwidGVzdDIvIyIsInRlc3QzLyMiXSwicmVhZCI6WyJ0ZXN0MS8jIiwidGVzdDIvIyIsInRlc3QzLyMiXSwiY3JlYXRlVG9rZW4iOlsidGVzdDEiLCJ0ZXN0MiIsInRlc3QzIl19.FSSecEiStcElHu0AqpmcIvfyMElwKZQUkiO5X_r0_3g";

    AccessController acs;
    bool token_loaded = acs.authenticate(token, "eventhub_secret");

    WHEN("Loading our token") {
      THEN("The token should be successfully loaded") {
        REQUIRE(token_loaded == true);
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
}
