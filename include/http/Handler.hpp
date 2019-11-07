#ifndef INCLUDE_HTTP_HANDLER_HPP_
#define INCLUDE_HTTP_HANDLER_HPP_

#include <memory>
#include <string>

#include "Connection.hpp"
#include "ConnectionWorker.hpp"
#include "HandlerContext.hpp"
#include "TopicManager.hpp"
#include "http/Parser.hpp"

namespace eventhub {
namespace http {
#define WS_MAGIC_STRING "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

class Handler {
public:
  static void HandleRequest(HandlerContext&& ctx, Parser* req, RequestState reqState);

private:
  Handler() {}
  ~Handler() {}

  static void _handlePath(HandlerContext& ctx, Parser* req);
  static bool _websocketHandshake(HandlerContext& ctx, Parser* req);
  static void _badRequest(HandlerContext& ctx, const std::string reason, int statusCode = 400);
};

} // namespace http
} // namespace eventhub

#endif // INCLUDE_HTTP_HANDLER_HPP_
