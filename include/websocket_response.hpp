#ifndef EVENTHUB_WEBSOCKET_RESPONSE_HPP
#define EVENTHUB_WEBSOCKET_RESPONSE_HPP

#include <string>

namespace eventhub {
  namespace websocket {
    class response {
      public:
        response(const std::string& data, uint8_t opcode = 0x1, uint8_t fin=1);
        ~response();

        const char* ws_format();

        private:
          char* _buf;
    };
  }
}

#endif
