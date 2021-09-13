#ifndef INCLUDE_RPCHANDLER_HPP_
#define INCLUDE_RPCHANDLER_HPP_

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "Connection.hpp"
#include "HandlerContext.hpp"
#include "jsonrpc/jsonrpcpp.hpp"

namespace eventhub {

using RPCMethod      = std::function<void(HandlerContext& hCtx, jsonrpcpp::request_ptr)>;
using RPCHandlerList = std::vector<std::pair<std::string, RPCMethod>>;

class RPCHandler final {
public:
  static RPCMethod getHandler(const std::string& methodName);

private:
  RPCHandler();
  ~RPCHandler();

  static void _sendSuccessResponse(HandlerContext& hCtx, jsonrpcpp::request_ptr req, const nlohmann::json& result);
  static void _sendInvalidParamsError(HandlerContext& hCtx, jsonrpcpp::request_ptr req, const std::string& message);

  static void _handleSubscribe(HandlerContext& hCtx, jsonrpcpp::request_ptr req);
  static void _handleUnsubscribe(HandlerContext& hCtx, jsonrpcpp::request_ptr req);
  static void _handleUnsubscribeAll(HandlerContext& hCtx, jsonrpcpp::request_ptr req);
  static void _handlePublish(HandlerContext& hCtx, jsonrpcpp::request_ptr req);
  static void _handleList(HandlerContext& hCtx, jsonrpcpp::request_ptr req);
  static void _handleHistory(HandlerContext& hCtx, jsonrpcpp::request_ptr req);
  static void _handlePing(HandlerContext& hCtx, jsonrpcpp::request_ptr req);
  static void _handleDisconnect(HandlerContext& hCtx, jsonrpcpp::request_ptr req);
};

} // namespace eventhub

#endif // INCLUDE_RPCHANDLER_HPP_
