#ifndef EVENTHUB_WEBSOCKET_RESPONSE_HPP

#include "Connection.hpp"
#include "websocket/Types.hpp"
#include <memory>
#include <string>

namespace eventhub {
namespace websocket {
namespace response {

void sendData(ConnectionPtr conn, const std::string& data, FrameType frameType);

} // namespace response
} // namespace websocket
} // namespace eventhub

#endif
