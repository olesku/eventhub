#include "Config.hpp"
#include "catch.hpp"
#include <cstdlib>

using namespace eventhub;
using namespace std;

TEST_CASE("Test config class", "[config]") {
  GIVEN("That we set a integer env variable.") {
    THEN("We should be able to retrieve it though our config object.") {
      //Config.addOption<int>("LISTEN_PORT", 8080);
      //REQUIRE(Config.get<int>("LISTEN_PORT") == 8080);

      setenv("LISTEN_PORT", "9090", 1);
      Config.addOption<int>("LISTEN_PORT", 8080);
      REQUIRE(Config.get<int>("LISTEN_PORT") == 9090);
    }
  }
}
