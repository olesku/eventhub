#include "websocket_response.hpp"
#include "common.hpp"
#include <arpa/inet.h>
#include <string.h>

using namespace std;

namespace eventhub {
WebsocketResponse::WebsocketResponse(const string& data, uint8_t opcode, uint8_t fin) {
  char header[8];
  size_t header_size = 0;

  header[0] = fin << 7;
  header[0] = header[0] | (0xF & opcode);
  header[1] = 0x0 << 7; // No mask.

  if (data.length() < 126) {
    header[1]   = header[1] | data.length();
    header_size = 2;
  } else if (data.length() < 0xFFFF) {
    header[1]   = header[1] | 0x7E;
    uint16_t sz = htons(static_cast<uint16_t>(data.length()));
    memcpy(header + 2, &sz, sizeof(uint16_t));
    header_size = 4;
  } else {
    header[1]   = header[1] | 0x7F;
    uint16_t sz = htonl(static_cast<uint16_t>(data.length()));
    memcpy(header + 2, &sz, sizeof(uint16_t));
    header_size = 8;
  }

  _sbuf.insert(0, header, header_size);
  _sbuf.append(data);
}

WebsocketResponse::~WebsocketResponse() {
}

const std::string& WebsocketResponse::ws_format() {
  return _sbuf;
}
} // namespace eventhub
