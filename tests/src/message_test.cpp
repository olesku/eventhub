#include "catch.hpp"
#include "common.hpp"
#include "message.hpp"
#include <iostream>

TEST_CASE("Test message", "[message]") {
  SECTION("Test stuff") {
    eventhub::Message msg("topic1", "Dette er en test");

    std::cout << msg.get() << std::endl;

    REQUIRE(1==1);
  }
}
