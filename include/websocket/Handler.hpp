#ifndef EVENTHUB_WEBSOCKET_HANDLER_HPP
#define EVENTHUB_WEBSOCKET_HANDLER_HPP

#include "Connection.hpp"
#include "ConnectionWorker.hpp"
#include "websocket/Types.hpp"
#include "HandlerContext.hpp"
#include <memory>
#include <vector>
#include <functional>

namespace eventhub {
namespace websocket {

class Handler {
public:
  static void HandleRequest(websocket::ParserStatus parserStatus, websocket::FrameType frameType, const std::string& data, HandlerContext& ctx);

private:
  Handler(){};
  ~Handler(){};

  static void _handleTextFrame(HandlerContext& ctx, const std::string& data);
};

} // namespace websocket
} // namespace eventhub

#endif
