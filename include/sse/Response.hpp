#ifndef INCLUDE_SSE_RESPONSE_HPP_
#define INCLUDE_SSE_RESPONSE_HPP_

#include <memory>
#include <string>

#include "Connection.hpp"

namespace eventhub {
namespace sse {
namespace response {

void ok(ConnectionPtr conn);
void sendPing(ConnectionPtr conn);
void sendEvent(ConnectionPtr conn, const std::string& id, const std::string& message, const std::string event="");
void error(ConnectionPtr conn, const std::string& message, unsigned int statusCode=404);
} // namespace response
} // namespace SSE
} // namespace eventhub

#endif // INCLUDE_SSE_RESPONSE_HPP_
