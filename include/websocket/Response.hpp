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
    static void sendData(ConnectionPtr conn, const std::string& data, FrameType frameType);

  private:
    static void _sendFragment(ConnectionPtr conn, const std::string& fragment, uint8_t frameType, bool fin);
};

} // namespace websocket
} // namespace eventhub


