#pragma once

#include <memory>
#include <string>

#include "Connection.hpp"

namespace eventhub {
namespace sse {

class Response final {
  public:
    static void ok(ConnectionPtr conn);
    static void sendPing(ConnectionPtr conn);
    static void sendEvent(ConnectionPtr conn, const std::string& id, const std::string& message, const std::string& event = "");
    static void error(ConnectionPtr conn, const std::string& message, std::size_t statusCode = 404);
};

} // namespace sse
} // namespace eventhub


