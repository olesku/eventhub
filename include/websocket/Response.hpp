#ifndef INCLUDE_WEBSOCKET_RESPONSE_HPP_
#define INCLUDE_WEBSOCKET_RESPONSE_HPP_

#include <memory>
#include <string>

#include "Connection.hpp"
#include "websocket/Types.hpp"

namespace eventhub {
namespace websocket {
namespace response {

void sendData(ConnectionPtr conn, const std::string& data, FrameType frameType);

} // namespace response
} // namespace websocket
} // namespace eventhub

#endif // INCLUDE_WEBSOCKET_RESPONSE_HPP_
