#pragma once

#include <string>

#include "Forward.hpp"
#include "websocket/Types.hpp"

namespace eventhub {

namespace websocket {

class Handler final {
public:
  static void HandleRequest(HandlerContext&& ctx, FrameType frameType, const std::string& data);
  static void HandleError(HandlerContext&& ctx, ParserError error);

private:
  Handler() = delete;

  static void _handleTextFrame(HandlerContext& ctx, const std::string& data);
};

} // namespace websocket
} // namespace eventhub
