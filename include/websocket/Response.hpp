#pragma once

#include <memory>
#include <stdint.h>
#include <string>

#include "Connection.hpp"
#include "websocket/Types.hpp"

namespace eventhub {
namespace websocket {

class Response final {
public:
  static bool sendData(ConnectionPtr connection, const std::string& data, FrameType frameType);

private:
  static void _appendFragment(std::string& output, std::string_view fragment,
                              std::uint8_t frameType, bool final);
};

} // namespace websocket
} // namespace eventhub
