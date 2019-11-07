#ifndef INCLUDE_WEBSOCKET_HANDLER_HPP_
#define INCLUDE_WEBSOCKET_HANDLER_HPP_

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "Connection.hpp"
#include "ConnectionWorker.hpp"
#include "HandlerContext.hpp"
#include "websocket/Types.hpp"

namespace eventhub {
namespace websocket {

class Handler {
public:
  static void HandleRequest(HandlerContext&& ctx, websocket::ParserStatus parserStatus, websocket::FrameType frameType, const std::string& data);

private:
  Handler() {}
  ~Handler() {}

  static void _handleTextFrame(HandlerContext& ctx, const std::string& data);
};

} // namespace websocket
} // namespace eventhub

#endif // INCLUDE_WEBSOCKET_HANDLER_HPP_
