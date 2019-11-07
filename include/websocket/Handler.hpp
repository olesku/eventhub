#ifndef INCLUDE_WEBSOCKET_HANDLER_HPP_
#define INCLUDE_WEBSOCKET_HANDLER_HPP_

#include <string>
#include <memory>
#include <vector>
#include <functional>

#include "Connection.hpp"
#include "ConnectionWorker.hpp"
#include "websocket/Types.hpp"
#include "HandlerContext.hpp"

namespace eventhub {
namespace websocket {

class Handler {
public:
  static void HandleRequest(HandlerContext&& ctx, websocket::ParserStatus parserStatus, websocket::FrameType frameType, const std::string& data);

private:
  Handler(){}
  ~Handler(){}

  static void _handleTextFrame(HandlerContext& ctx, const std::string& data);
};

} // namespace websocket
} // namespace eventhub

#endif // INCLUDE_WEBSOCKET_HANDLER_HPP_
