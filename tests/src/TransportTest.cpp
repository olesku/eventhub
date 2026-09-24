#include <array>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include "Transport.hpp"
#include "catch.hpp"

namespace eventhub {

TEST_CASE("TCP transport distinguishes progress blocking and EOF", "[transport]") {
  std::array<int, 2> sockets{};
  REQUIRE(socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0, sockets.data()) == 0);
  auto transport = makeTcpTransport(SocketHandle(sockets[0]));
  SocketHandle peer(sockets[1]);

  char buffer[16]{};
  auto result = transport->read(buffer, sizeof(buffer));
  REQUIRE(result.status == IoStatus::WOULD_BLOCK);
  REQUIRE(result.wait == IoWait::READ);

  REQUIRE(::write(peer.get(), "hello", 5) == 5);
  result = transport->read(buffer, sizeof(buffer));
  REQUIRE(result.status == IoStatus::PROGRESS);
  REQUIRE(result.bytes == 5);
  REQUIRE(std::string(buffer, result.bytes) == "hello");

  peer.reset();
  result = transport->read(buffer, sizeof(buffer));
  REQUIRE(result.status == IoStatus::EOF_REACHED);
}

} // namespace eventhub
