#include "websocket/Handler.hpp"

#include <functional>
#include <string>
#include <vector>

#include "Common.hpp"
#include "Config.hpp"
#include "Connection.hpp"
#include "HandlerContext.hpp"
#include "RPCHandler.hpp"
#include "jsonrpc/jsonrpcpp.hpp"
#include "jwt/json/json.hpp"
#include "websocket/Response.hpp"
#include "websocket/Types.hpp"

namespace eventhub {
namespace websocket {

/**
 * Process incoming websocket requests and call the correct handlers.
 * @param parserStatus Websocket parser status.
 * @param frameType Websoket request frame type.
 * @param data Request data.
 * @param ctx HandlerContext (server, worker, client).
 */
void Handler::HandleRequest(HandlerContext&& ctx, ParserStatus parserStatus, FrameType frameType,
                            const std::string& data) {
  switch (parserStatus) {
    case ParserStatus::PARSER_OK:
      break;

    case ParserStatus::MAX_DATA_FRAME_SIZE_EXCEEDED:
      LOG(INFO) << "Client " << ctx.connection()->getIP() << " exceeded max data frame size, hanging up.";
      response::sendData(ctx.connection(), "", websocket::FrameType::CLOSE_FRAME);
      ctx.connection()->shutdown();
      return;
      break;

    case ParserStatus::MAX_CONTROL_FRAME_SIZE_EXCEEDED:
      LOG(INFO) << "Client " << ctx.connection()->getIP() << " exceeded max control frame size, hanging up.";
      response::sendData(ctx.connection(), "", websocket::FrameType::CLOSE_FRAME);
      ctx.connection()->shutdown();
      return;
      break;
  }

  switch (frameType) {
    case FrameType::TEXT_FRAME:
      // response::sendData(ctx.connection(), data, FrameType::TEXT_FRAME);
      _handleTextFrame(ctx, data);
      break;

    case FrameType::BINARY_FRAME:
      // Not supported yet.
      //DLOG(INFO) << "Client " << ctx.connection()->getIP() << " received a binary frame.";
      break;

    case FrameType::PING_FRAME:
      //LOG(INFO) << "Got PING request from " << ctx.connection()->getIP();
      response::sendData(ctx.connection(), data, FrameType::PONG_FRAME);
      break;

    case FrameType::PONG_FRAME:
      //LOG(INFO) << "Got PONG request from " << ctx.connection()->getIP();
      break;

    case FrameType::CLOSE_FRAME:
      ctx.connection()->shutdown();
      break;

    case FrameType::CONTINUATION_FRAME:
      break;
  }
}

/**
 * Handle websocket data frame.
 * @param conn Connection
 */
void Handler::_handleTextFrame(HandlerContext& ctx, const std::string& data) {
  // TODO: Not thread safe, should instantiate the Handler class.
  static jsonrpcpp::Parser parser;
  jsonrpcpp::entity_ptr entity;

  try {
    entity = parser.parse(data);
  } catch (std::exception& e) {
    DLOG(INFO) << "Failed to parse RPC request from " << ctx.connection()->getIP() << ": " << e.what();
    response::sendData(ctx.connection(),
                       jsonrpcpp::Response(jsonrpcpp::InvalidRequestException("Invalid request")).to_json().dump(),
                       websocket::FrameType::TEXT_FRAME);
    return;
  }

  if (entity && entity->is_request()) {
    auto req = dynamic_pointer_cast<jsonrpcpp::Request>(entity);
    try {
      auto handler = RPCHandler::getHandler(req->method());
      //DLOG(INFO) << ctx.connection()->getIP() << ":";
      //DLOG(INFO) << req->to_json().dump(2);
      handler(ctx, req);
    } catch (std::exception& e) {
      DLOG(ERROR) << "Invalid RPC method called by " << ctx.connection()->getIP() << ": " << e.what();
      response::sendData(ctx.connection(),
                         jsonrpcpp::Response(jsonrpcpp::MethodNotFoundException(*req)).to_json().dump(),
                         websocket::FrameType::TEXT_FRAME);
    }
  } else {
    DLOG(ERROR) << "Invalid RPC request by " << ctx.connection()->getIP();
    response::sendData(ctx.connection(),
                       jsonrpcpp::Response(jsonrpcpp::InvalidRequestException("Invalid request")).to_json().dump(),
                       websocket::FrameType::TEXT_FRAME);
  }
}

} // namespace websocket
} // namespace eventhub
