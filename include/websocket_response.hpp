#ifndef EVENTHUB_WEBSOCKET_RESPONSE_HPP
#define EVENTHUB_WEBSOCKET_RESPONSE_HPP

#include <string>

namespace eventhub {
class WebsocketResponse {
public:
  WebsocketResponse(const std::string& data, uint8_t opcode = 0x1, uint8_t fin = 1);
  ~WebsocketResponse();

  const std::string& ws_format();

private:
  std::string _sbuf;
};
} // namespace eventhub

#endif
