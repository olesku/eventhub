#ifndef EVENTHUB_WEBSOCKET_HANDLER_HPP
#define EVENTHUB_WEBSOCKET_HANDLER_HPP

#include "connection.hpp"
#include <memory>

namespace eventhub {
  class websocket_handler {
    #define MAGIC_STRING "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
    public:
      static connection::state handshake(const std::shared_ptr<connection>& conn);
      static connection::state parse(const std::shared_ptr<connection>& conn, const char* buf, size_t buf_len);
    
    private:
      websocket_handler() {};
      static void _bad_request(const std::shared_ptr<connection>& conn);
  };
}

#endif
