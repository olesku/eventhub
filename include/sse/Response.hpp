#ifndef INCLUDE_SSE_RESPONSE_HPP_
#define INCLUDE_SSE_RESPONSE_HPP_

#include <memory>
#include <string>

#include "Connection.hpp"

namespace eventhub {
namespace sse {
namespace response {

void sendPing(ConnectionPtr conn);

} // namespace response
} // namespace SSE
} // namespace eventhub

#endif // INCLUDE_SSE_RESPONSE_HPP_
