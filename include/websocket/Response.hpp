#pragma once

#include <string>

#include "Connection.hpp"
#include "Forward.hpp"
#include "websocket/Types.hpp"

namespace eventhub {
namespace websocket {

class Response final {
public:
  static void sendData(ConnectionPtr conn, const std::string& data, FrameType frameType);
  static void sendCompressibleText(Connection& conn, const std::string& data);
  static void sendEvent(Connection& conn, const jsonrpcpp::Id& id, const PublishedEvent& event);
};

} // namespace websocket
} // namespace eventhub
