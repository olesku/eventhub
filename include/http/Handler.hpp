#pragma once

#include <memory>
#include <string>

#include "Forward.hpp"

namespace eventhub {
namespace http {

class Handler final {
public:
  static void HandleRequest(HandlerContext&& ctx, Parser* req, RequestState reqState);

private:
  Handler() {}
  ~Handler() {}

  static void _handlePath(HandlerContext& ctx, Parser* req);
  static bool _websocketHandshake(HandlerContext& ctx, Parser* req);
  static void _badRequest(HandlerContext& ctx, std::string_view reason, int statusCode = 400);
  static void _setCorsHeaders(Parser* req, Response& resp);
};

} // namespace http
} // namespace eventhub


