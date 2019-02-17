#ifndef EVENTHUB_WEBSOCKET_HANDLER_HPP
#define EVENTHUB_WEBSOCKET_HANDLER_HPP

#include <memory>
#include "connection.hpp"
#include "connection_worker.hpp"
#include "websocket_request.hpp"

namespace eventhub {
  class websocket_handler {
    public:
      static void parse(std::shared_ptr<io::connection>& conn, char* buf, size_t n_bytes);

    private:
      websocket_handler() {};
      ~websocket_handler() {};
      
      static void _handle_data_frame(std::shared_ptr<io::connection>& conn, websocket_request& req);
      static void _handle_control_frame(std::shared_ptr<io::connection>& conn, websocket_request& req);
  };
}

#endif
