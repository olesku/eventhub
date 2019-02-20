#ifndef EVENTHUB_HTTP_HANDLER_HPP
#define EVENTHUB_HTTP_HANDLER_HPP

#include <memory>
#include "connection.hpp"
#include "connection_worker.hpp"

namespace eventhub {
  #define WS_MAGIC_STRING "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

  class http_handler {
    public:
      static void parse(std::shared_ptr<io::connection>& conn, const char* buf, size_t n_bytes);

    private:
      http_handler() {};
      ~http_handler() {};

      static void _handle_path(std::shared_ptr<io::connection>& conn, http_request& req);
      static bool _websocket_handshake(std::shared_ptr<io::connection>& conn, http_request& req);
      static void _bad_request(std::shared_ptr<io::connection>& conn, const std::string reason);
  };
}

#endif
