#ifndef EVENTHUB_WEBSOCKET_HANDLER_HPP
#define EVENTHUB_WEBSOCKET_HANDLER_HPP

#include "connection.hpp"
#include <memory>

namespace eventhub {
  class websocket_handler {
    #define MAGIC_STRING "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
    public:
      websocket_handler();
      ~websocket_handler();

      static bool handshake(const std::shared_ptr<connection>& conn);
    
    private:
      static void _bad_request(const std::shared_ptr<connection>& conn);
  };
}

#endif
