#ifndef EVENTHUB_HTTP_HANDLER_HPP
#define EVENTHUB_HTTP_HANDLER_HPP

#include "Connection.hpp"
#include "ConnectionWorker.hpp"
#include "TopicManager.hpp"
#include "http/RequestStateMachine.hpp"
#include <memory>

namespace eventhub {
namespace http {
#define WS_MAGIC_STRING "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

class Handler {
public:
  static void process(ConnectionPtr conn, const char* buf, size_t nBytes);

private:
  Handler(){};
  ~Handler(){};

  static void _handlePath(ConnectionPtr conn);
  static bool _websocketHandshake(ConnectionPtr conn);
  static void _badRequest(ConnectionPtr conn, const std::string reason, int statusCode = 400);
};

} // namespace http
} // namespace eventhub

#endif