#ifndef INCLUDE_SSE_RESPONSE_HPP_
#define INCLUDE_SSE_RESPONSE_HPP_

#include "sse/Response.hpp"

#include <memory>
#include <string>

#include "Connection.hpp"

namespace eventhub {
namespace sse {
namespace response {

void sendPing(ConnectionPtr conn) {
  conn->write(":\n\n");
}

} // namespace response
} // namespace sse
} // namespace eventhub

#endif // INCLUDE_SSE_RESPONSE_HPP_
