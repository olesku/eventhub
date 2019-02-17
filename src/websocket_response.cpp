#include <arpa/inet.h>
#include <string.h>
#include "websocket_response.hpp"

using namespace std;

namespace eventhub {
  namespace websocket {
    response::response(const string& data, uint8_t opcode, uint8_t fin) {
      size_t bufsize = data.length();
      size_t headsize = 0;

      if (bufsize < 126) {
        bufsize += 2;  // fin = 1bit, rsv = 3bit, opcode = 4bit, mask = 1 bit, payload_len = 7bit == 16bit or 2 bytes.
      } else if (bufsize < 0xFFFF) {
        bufsize += 4;
      } else {
        bufsize += 8;
      }

      _buf = new char(bufsize); //(char*)malloc(sizeof(char) * bufsize);

      *_buf = fin << 7;
      *_buf = *_buf | (0xF & opcode);
      *(_buf+1) = 0x0 << 7; // No mask.

      if (data.length() < 126) {
        *(_buf+1) = *(_buf+1) | data.length();
        headsize = 2;
      } else if (data.length() < 0xFFFF) {
        *(_buf+1) = *(_buf+1) | 0x7E;
        uint16_t sz = htons(static_cast<uint16_t>(data.length()));
        memcpy(_buf+2, &sz, sizeof(uint16_t));
        headsize = 4;
      } else {
        *(_buf+1) = *(_buf+1) | 0x7F;
        uint16_t sz = htonl(static_cast<uint16_t>(data.length()));
        memcpy(_buf+2, &sz, sizeof(uint16_t));
        headsize = 8;
      }

      memcpy(_buf+headsize, data.c_str(), data.length());
      _buf[bufsize] = '\0';
    }

    response::~response() {
      delete(_buf);
    }

    const char* response::ws_format() {
      return _buf;
    }
  }
}
