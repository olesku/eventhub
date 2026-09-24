#pragma once

#include <memory>
#include <string>

#include "Forward.hpp"

namespace eventhub {
namespace http {

class Handler final {
public:
  static void handleRequest(HandlerContext&& ctx, const Request& request);
  static void handleError(HandlerContext&& ctx, ParseError error);

private:
  Handler() {}
  ~Handler() {}

  static void _handlePath(HandlerContext& ctx, const Request& request);
  static bool _websocketHandshake(HandlerContext& ctx, const Request& request);
  static void _badRequest(HandlerContext& ctx, const std::string& reason, int statusCode = 400);
  static void _setCorsHeaders(const Request& request, Response& response);
};

} // namespace http
} // namespace eventhub
