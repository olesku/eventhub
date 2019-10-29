#ifndef EVENTHUB_WEBSOCKET_RESPONSE_HPP

#include "Connection.hpp"
#include <memory>
#include <string>

namespace eventhub {
namespace websocket {
namespace response {
enum Opcodes {
  CONTINUATION_FRAME = 0x0,
  TEXT_FRAME         = 0x1,
  BINARY_FRAME       = 0x2,
  CLOSE_FRAME        = 0x8,
  PING_FRAME         = 0x9,
  PONG_FRAME         = 0xA
};

// 32K fragment size.
static constexpr size_t WS_MAX_CHUNK_SIZE = 1 << 15;

void sendData(ConnectionPtr conn, const std::string& data, uint8_t opcode);

} // namespace response
} // namespace websocket
} // namespace eventhub

#endif
