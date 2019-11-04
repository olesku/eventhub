#include "Config.hpp"
#include "catch.hpp"
#include <string>
#include <cstdlib>

using namespace eventhub;
using namespace eventhub::config;

// Integers
TEST_CASE("Integer type configuration option test" "[config]") {
  GIVEN("That we set a default value") {
    Config.add<int>("MY_INT_PARAM_1", 1000);
    THEN("We should get that value back if the env is unset.") {
      REQUIRE(Config.get<int>("MY_INT_PARAM_1") == 1000);
    }
  }

  GIVEN("That the environment has a variable set") {
    setenv("MY_INT_PARAM_2", "2000", 1);
    Config.add<int>("MY_INT_PARAM_2", 1000);
    THEN("We should get the value of that env and not the default value") {
      REQUIRE(Config.get<int>("MY_INT_PARAM_2") == 2000);
    }
  }

  GIVEN("That a integer config env has a non-numeric value") {
    setenv("MY_INIT_PARAM_3", "BADVALUE", 1);
    THEN("We should get an exception when we call add") {
      CHECK_THROWS_AS(Config.add<int>("MY_INIT_PARAM_3", 1000), InvalidValue);
    }
  }

  GIVEN("That we add the same config option twice") {
    THEN("We should get an exception on the second call") {
      Config.add<int>("MY_INIT_PARAM_4", 1000);
      CHECK_THROWS_AS(Config.add<int>("MY_INIT_PARAM_4", 1000), AlreadyAdded);
    }
  }

  GIVEN("That we request a wrong type for a config option") {
    THEN("We should fail with an exception") {
      Config.add<int>("MY_INIT_PARAM_5", 1000);
      CHECK_THROWS_AS(Config.get<std::string>("MY_INIT_PARAM_5"), InvalidTypeRequested);
      CHECK_THROWS_AS(Config.get<bool>("MY_INIT_PARAM_5"), InvalidTypeRequested);
    }
  }
}

// Strings
TEST_CASE("Test string configuration options" "[config]") {
  GIVEN("That we set a default value") {
    Config.add<std::string>("MY_STRING_PARAM_1", "Test string 1");
    THEN("We should get that value back if the env is unset.") {
      REQUIRE(Config.get<std::string>("MY_STRING_PARAM_1") == "Test string 1");
    }
  }

  GIVEN("That the environment has a variable set") {
    setenv("MY_STRING_PARAM_2", "Test string 2", 1);
    Config.add<std::string>("MY_STRING_PARAM_2", "Foobar");
    THEN("We should get the value of that env and not the default value") {
      REQUIRE(Config.get<std::string>("MY_STRING_PARAM_2") == "Test string 2");
    }
  }

  GIVEN("That we add the same config option twice") {
    THEN("We should get an exception on the second call") {
      Config.add<std::string>("MY_STRING_PARAM_3", "Test string 3");
      CHECK_THROWS_AS(Config.add<std::string>("MY_STRING_PARAM_3", "Test string 3"), AlreadyAdded);
    }
  }

  GIVEN("That we request a wrong type for a config option") {
    THEN("We should fail with an exception") {
      Config.add<std::string>("MY_STRING_PARAM_4", "Test string 4");
      CHECK_THROWS_AS(Config.get<int>("MY_STRING_PARAM_4"), InvalidTypeRequested);
      CHECK_THROWS_AS(Config.get<bool>("MY_STRING_PARAM_4"), InvalidTypeRequested);
    }
  }
}

// Booleans
TEST_CASE("Test boolean configuration options" "[config]") {
  GIVEN("That we set default values") {
    Config.add<bool>("MY_BOOL_PARAM_1", true);
    Config.add<bool>("MY_BOOL_PARAM_2", false);
    THEN("We should get that value back if the env is unset.") {
      REQUIRE(Config.get<bool>("MY_BOOL_PARAM_1") == true);
      REQUIRE(Config.get<bool>("MY_BOOL_PARAM_2") == false);
    }
  }

  GIVEN("That we set some config variables to true through env variables") {
    setenv("MY_BOOL_PARAM_TRUE_1", "true", 1);
    setenv("MY_BOOL_PARAM_TRUE_2", "TRUE", 1);
    setenv("MY_BOOL_PARAM_TRUE_3", "1", 1);
    Config.add<bool>("MY_BOOL_PARAM_TRUE_1", false);
    Config.add<bool>("MY_BOOL_PARAM_TRUE_2", false);
    Config.add<bool>("MY_BOOL_PARAM_TRUE_3", false);
    THEN("They should all be true") {
      REQUIRE(Config.get<bool>("MY_BOOL_PARAM_TRUE_1") == true);
      REQUIRE(Config.get<bool>("MY_BOOL_PARAM_TRUE_2") == true);
      REQUIRE(Config.get<bool>("MY_BOOL_PARAM_TRUE_3") == true);
    }
  }

  GIVEN("That we set some config variables to false through env variables") {
    setenv("MY_BOOL_PARAM_FALSE_1", "false", 1);
    setenv("MY_BOOL_PARAM_FALSE_2", "FALSE", 1);
    setenv("MY_BOOL_PARAM_FALSE_3", "0", 1);
    Config.add<bool>("MY_BOOL_PARAM_FALSE_1", true);
    Config.add<bool>("MY_BOOL_PARAM_FALSE_2", true);
    Config.add<bool>("MY_BOOL_PARAM_FALSE_3", true);
    THEN("They should all be false") {
      REQUIRE(Config.get<bool>("MY_BOOL_PARAM_FALSE_1") == false);
      REQUIRE(Config.get<bool>("MY_BOOL_PARAM_FALSE_2") == false);
      REQUIRE(Config.get<bool>("MY_BOOL_PARAM_FALSE_3") == false);
    }
  }

  GIVEN("That we add the same config option twice") {
    THEN("We should get an exception on the second call") {
      Config.add<bool>("MY_BOOL_PARAM_3", true);
      CHECK_THROWS_AS(Config.add<bool>("MY_BOOL_PARAM_3", true), AlreadyAdded);
    }
  }

  GIVEN("That we request a wrong type for a config option") {
    THEN("We should fail with an exception") {
      Config.add<bool>("MY_BOOL_PARAM_4", true);
      CHECK_THROWS_AS(Config.get<std::string>("MY_BOOL_PARAM_4"), InvalidTypeRequested);
      CHECK_THROWS_AS(Config.get<int>("MY_BOOL_PARAM_4"), InvalidTypeRequested);
    }
  }

  GIVEN("That we set the env variable to a non boolean value") {
    THEN("We should get an exception") {
      setenv("MY_BOOL_PARAM_5", "foobar", 1);
      CHECK_THROWS_AS(Config.add<bool>("MY_BOOL_PARAM_5", true), InvalidValue);
    }
  }
}