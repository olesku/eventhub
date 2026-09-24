#include <exception>
#include <functional>
#include <memory>
#include <spdlog/logger.h>
#include <string>

#include "Connection.hpp"
#include "HandlerContext.hpp"
#include "Logger.hpp"
#include "RPCHandler.hpp"
#include "jsonrpc/jsonrpcpp.hpp"
#include "websocket/Handler.hpp"
#include "websocket/Response.hpp"
#include "websocket/Types.hpp"

namespace eventhub {
namespace websocket {

/**
 * Process incoming websocket requests and call the correct handlers.
 * @param frameType WebSocket message or control frame type.
 * @param data Request data.
 * @param ctx HandlerContext (server, worker, client).
 */
void Handler::handleMessage(HandlerContext&& ctx, FrameType frameType, const std::string& data) {
  switch (frameType) {
    case FrameType::TEXT_FRAME:
      _handleTextFrame(ctx, data);
      break;

    case FrameType::BINARY_FRAME:
      // Not supported yet.
      break;

    case FrameType::PING_FRAME:
      Response::sendData(ctx.connection(), data, FrameType::PONG_FRAME);
      break;

    case FrameType::PONG_FRAME:
      break;

    case FrameType::CLOSE_FRAME:
      Response::sendData(ctx.connection(), data, FrameType::CLOSE_FRAME);
      ctx.connection()->closeAfterFlush();
      break;

    case FrameType::CONTINUATION_FRAME:
      break;
  }
}

void Handler::handleError(HandlerContext&& ctx, ParserError error) {
  LOG->debug("WebSocket error from {}: {}. Closing connection.", ctx.connection()->getIP(), errorMessage(error));
  std::uint16_t closeCode = 1002; // Protocol error.
  if (error == ParserError::MESSAGE_TOO_BIG) {
    closeCode = 1009;
  } else if (error == ParserError::INVALID_UTF8) {
    closeCode = 1007;
  }
  const std::string payload{static_cast<char>(closeCode >> 8), static_cast<char>(closeCode & 0xff)};
  Response::sendData(ctx.connection(), payload, FrameType::CLOSE_FRAME);
  ctx.connection()->closeAfterFlush();
}

/**
 * Handle websocket data frame.
 * @param conn Connection
 */
void Handler::_handleTextFrame(HandlerContext& ctx, const std::string& data) {
  thread_local jsonrpcpp::Parser parser;
  jsonrpcpp::entity_ptr entity;

  try {
    entity = parser.parse(data);
  } catch (std::exception& e) {
    LOG->debug("Failed to parse RPC request from {}: {}.", ctx.connection()->getIP(), e.what());
    Response::sendData(ctx.connection(),
                       jsonrpcpp::Response(jsonrpcpp::InvalidRequestException("Invalid request")).to_json().dump(),
                       websocket::FrameType::TEXT_FRAME);
    return;
  }

  if (entity && entity->is_request()) {
    auto req = std::dynamic_pointer_cast<jsonrpcpp::Request>(entity);
    try {
      auto handler = RPCHandler::getHandler(req->method());
      handler(ctx, req);
    } catch (std::exception& e) {
      LOG->debug("Invalid RPC method called by '{}': {}.", ctx.connection()->getIP(), e.what());
      Response::sendData(ctx.connection(),
                         jsonrpcpp::Response(jsonrpcpp::MethodNotFoundException(*req)).to_json().dump(),
                         websocket::FrameType::TEXT_FRAME);
    }
  } else {
    LOG->debug("Invalid RPC request by {}.", ctx.connection()->getIP());
    Response::sendData(ctx.connection(),
                       jsonrpcpp::Response(jsonrpcpp::InvalidRequestException("Invalid request")).to_json().dump(),
                       websocket::FrameType::TEXT_FRAME);
  }
}

} // namespace websocket
} // namespace eventhub
