#pragma once

#include <stdint.h>
#include <memory>
#include <string>

#include "Connection.hpp"
#include "websocket/Types.hpp"

namespace eventhub {
namespace websocket {

class Response final {
  public:
    static void sendData(ConnectionPtr conn, std::string_view data, FrameType frameType);

  private:
    static void _sendFragment(ConnectionPtr conn, std::string_view fragment, uint8_t frameType, bool fin);
};

} // namespace websocket
} // namespace eventhub


