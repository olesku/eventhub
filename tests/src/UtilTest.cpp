#include <unistd.h>
#include <fstream>
#include <string>

#include "Util.hpp"
#include "catch.hpp"

namespace eventhub {

TEST_CASE("Util test") {
  SECTION("Test getFileMD5Sum") {
    unlink("md5testfile");
    std::ofstream f;
    f.open("md5testfile");
    f << "This is a MD5 string";
    f.close();

    auto sha1Sum = Util::getFileMD5Sum("md5testfile");
    unlink("md5testfile");
    REQUIRE(sha1Sum == "65ee3eeff640758f8c1584a6bb9ff6b4");
  }
}

}
