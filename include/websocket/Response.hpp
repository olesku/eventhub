#ifndef EVENTHUB_WEBSOCKET_RESPONSE_HPP

#include <memory>
#include <string>
#include "Connection.hpp"

namespace eventhub {
namespace websocket {
namespace response {
enum Opcodes {
  CONTINUTION_FRAME = 0x0,
  TEXT_FRAME        = 0x1,
  BINARY_FRAME      = 0x2,
  CLOSE_FRAME       = 0x8,
  PING_FRAME        = 0x9,
  PONG_FRAME        = 0xA
};

void sendData(std::shared_ptr<Connection>& conn, const std::string& data, uint8_t opcode=Opcodes::TEXT_FRAME, uint8_t fin=1);

}
}
}

#endif
