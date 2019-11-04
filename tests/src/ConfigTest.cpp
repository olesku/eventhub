#include "Config.hpp"
#include "catch.hpp"
#include <cstdlib>

using namespace eventhub;
using namespace std;

TEST_CASE("Integer configuration param test" "[config]") {
  GIVEN("That we set a default value") {
    Config.addOption<int>("MY_INT_PARAM_1", 1000);
    THEN("We should get that value back if the env is unset.") {
      REQUIRE(Config.get<int>("MY_INT_PARAM_1") == 1000);
    }
  }

  GIVEN("That the environment has a variable set") {
    setenv("MY_INT_PARAM_2", "2000", 1);
    Config.addOption<int>("MY_INT_PARAM_2", 1000);
    THEN("We should get the value of that env and not the default value") {
      REQUIRE(Config.get<int>("MY_INT_PARAM_2") == 2000);
    }
  }

  GIVEN("That a integer config env has a non numeric value") {
    setenv("MY_INIT_PARAM_3", "BADVALUE", 1);
    THEN("We should get an exception when we call addOption") {
      CHECK_THROWS_AS(Config.addOption<int>("MY_INIT_PARAM_3", 1000), InvalidValue);
    }
  }
}
