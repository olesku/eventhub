#ifndef INCLUDE_SSE_RESPONSE_HPP_
#define INCLUDE_SSE_RESPONSE_HPP_

#include <memory>
#include <string>

#include "Connection.hpp"

namespace eventhub {
namespace sse {

class Response {
  public:
    static void ok(ConnectionPtr conn);
    static void sendPing(ConnectionPtr conn);
    static void sendEvent(ConnectionPtr conn, const std::string& id, const std::string& message, const std::string event = "");
    static void error(ConnectionPtr conn, const std::string& message, unsigned int statusCode = 404);
};

} // namespace sse
} // namespace eventhub

#endif // INCLUDE_SSE_RESPONSE_HPP_
