#ifndef EVENTHUB_HTTP_HANDLER_HPP
#define EVENTHUB_HTTP_HANDLER_HPP

#include "connection.hpp"
#include "connection_worker.hpp"
#include "topic_manager.hpp"
#include <memory>

namespace eventhub {
#define WS_MAGIC_STRING "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

class HTTPHandler {
public:
  static void parse(std::shared_ptr<Connection>& conn, Worker* wrk, const char* buf, size_t n_bytes);

private:
  HTTPHandler(){};
  ~HTTPHandler(){};

  static void _handlePath(std::shared_ptr<Connection>& conn, Worker* wrk, HTTPRequest& req);
  static bool _websocketHandshake(std::shared_ptr<Connection>& conn, HTTPRequest& req);
  static void _badRequest(std::shared_ptr<Connection>& conn, const std::string reason);
};
} // namespace eventhub

#endif
